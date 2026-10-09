// Fast NTFS enumeration via FSCTL_ENUM_USN_DATA (MFT scan).
//
// Reads every MFT record on the volume in kernel order (no per-file Win32
// syscalls), resolves full paths from parent file-reference numbers, and
// emits the entries under the requested root. Falls back to walk_tree when
// the volume handle cannot be opened (needs elevation), the filesystem is
// not NTFS, or any step fails. Callers (upsert_file) re-stat, so only paths
// are produced here.

#include "wilfred/fs/walker.hpp"

#ifdef _WIN32
#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <windows.h>
#include <winioctl.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <unordered_map>
#include <vector>

namespace wilfred {
namespace {

constexpr std::uint64_t kUsnPageBytes = 65536;

std::uint16_t rd16(const std::uint8_t* p) {
  std::uint16_t v = 0;
  std::memcpy(&v, p, 2);
  return v;
}

std::uint32_t rd32(const std::uint8_t* p) {
  std::uint32_t v = 0;
  std::memcpy(&v, p, 4);
  return v;
}

std::uint64_t rd64(const std::uint8_t* p) {
  std::uint64_t v = 0;
  std::memcpy(&v, p, 8);
  return v;
}

// Parsed USN_RECORD_V2/V3 common prefix (offsets identical in both).
struct UsnRec {
  std::uint64_t frn{0};
  std::uint64_t parent{0};
  std::uint32_t attrs{0};
  std::wstring name;
};

bool parse_usn_record(const std::uint8_t* p, std::size_t avail, UsnRec& out,
                      std::uint32_t& reclen) {
  if (avail < 60) return false;
  std::uint32_t len = rd32(p);
  if (len < 60 || len > avail) return false;
  std::uint16_t major = rd16(p + 4);
  if (major != 2 && major != 3) return false;  // V4 has a different layout
  std::uint16_t name_len = rd16(p + 56);
  std::uint16_t name_off = rd16(p + 58);
  if (static_cast<std::uint32_t>(name_off) + name_len > len) return false;
  out.frn = rd64(p + 8);
  out.parent = rd64(p + 16);
  out.attrs = rd32(p + 52);
  out.name.assign(reinterpret_cast<const wchar_t*>(p + name_off), name_len / 2);
  reclen = (len + 7) & ~7u;  // records are 8-byte aligned
  if (reclen > avail) return false;
  return true;
}

std::wstring volume_mount_of(const std::wstring& root) {
  WCHAR vol[MAX_PATH]{};
  if (!GetVolumePathNameW(root.c_str(), vol, MAX_PATH)) return {};
  return vol;
}

}  // namespace

std::unordered_map<std::uint64_t, std::string> usn_resolve_paths(
    const std::unordered_map<std::uint64_t, UsnNode>& nodes, const std::string& volume_prefix) {
  std::unordered_map<std::uint64_t, std::string> done;
  done.reserve(nodes.size());
  std::vector<std::uint64_t> stack;
  for (auto& [frn, node] : nodes) {
    if (done.count(frn)) continue;
    stack.clear();
    std::uint64_t cur = frn;
    while (true) {
      if (done.count(cur)) break;
      auto it = nodes.find(cur);
      if (it == nodes.end()) break;
      if (it->second.parent == cur) {  // volume root points at itself
        done[cur] = volume_prefix;
        break;
      }
      stack.push_back(cur);
      cur = it->second.parent;
      if (stack.size() > static_cast<std::size_t>(4096)) break;  // corrupt cycle guard
    }
    while (!stack.empty()) {
      auto f = stack.back();
      stack.pop_back();
      auto nit = nodes.find(f);
      std::string base;
      auto dit = done.find(nit->second.parent);
      if (dit != done.end())
        base = dit->second;
      else
        base = volume_prefix;
      std::string rel = wide_to_utf8(nit->second.name);
      if (!base.empty() && base.back() != '\\' && base.back() != '/') base.push_back('\\');
      done[f] = base + rel;
    }
  }
  return done;
}

bool win_usn_enumerate_tree(const std::string& root, const Config& cfg, WalkFn on_entry,
                            std::atomic<bool>* cancel, WalkStats* stats) {
  auto wroot = utf8_to_wide(root);
  // Only fixed NTFS volumes qualify.
  auto mount = volume_mount_of(wroot);
  if (mount.empty()) return false;
  if (GetDriveTypeW(mount.c_str()) != DRIVE_FIXED) return false;
  WCHAR fsname[MAX_PATH]{};
  if (!GetVolumeInformationW(mount.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fsname,
                             MAX_PATH))
    return false;
  if (_wcsicmp(fsname, L"NTFS") != 0) return false;

  std::wstring vol = L"\\\\.\\" + std::wstring(mount.begin(), mount.end() - 1);  // \\.\C:
  HANDLE h = CreateFileW(vol.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    log_debug("walk", "USN unavailable, falling back to directory walk");
    return false;
  }

  // Sanity: journal presence (ENUM_USN_DATA works without one, but a broken
  // volume may fail here first).
  USN_JOURNAL_DATA journal{};
  DWORD br = 0;
  if (!DeviceIoControl(h, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &journal, sizeof(journal), &br,
                       nullptr)) {
    DWORD err = GetLastError();
    if (err != ERROR_JOURNAL_NOT_ACTIVE) {
      CloseHandle(h);
      return false;
    }
    // No journal: MFT enumeration still works.
  }

  std::unordered_map<std::uint64_t, UsnNode> nodes;
  nodes.reserve(512000);
  std::vector<std::uint8_t> page(kUsnPageBytes);
  MFT_ENUM_DATA_V1 med{};
  med.StartFileReferenceNumber = 0;
  med.LowUsn = 0;
  med.HighUsn = 0x7FFFFFFFFFFFFFFFLL;
  med.MinMajorVersion = 2;
  med.MaxMajorVersion = 3;
  bool ok = true;
  for (;;) {
    if (cancel && cancel->load()) break;
    DWORD out = 0;
    if (!DeviceIoControl(h, FSCTL_ENUM_USN_DATA, &med, sizeof(med), page.data(),
                         static_cast<DWORD>(page.size()), &out, nullptr)) {
      DWORD err = GetLastError();
      if (err == ERROR_HANDLE_EOF) break;
      log_debug("walk", "USN enumeration failed, falling back");
      ok = false;
      break;
    }
    if (out < sizeof(std::uint64_t)) break;
    std::memcpy(&med.StartFileReferenceNumber, page.data(), sizeof(std::uint64_t));
    std::size_t off = sizeof(std::uint64_t);
    while (off < out) {
      UsnRec rec;
      std::uint32_t reclen = 0;
      if (!parse_usn_record(page.data() + off, out - off, rec, reclen)) break;
      if (!rec.name.empty() && rec.name[0] != L'$') {
        // Skip $MFT, $LogFile, ... but keep normal dotfiles.
        UsnNode n;
        n.parent = rec.parent;
        n.name = std::move(rec.name);
        n.is_dir = (rec.attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
        nodes.emplace(rec.frn, std::move(n));
      }
      off += reclen;
      if (reclen == 0) break;
    }
    if (nodes.size() > 8000000) {
      log_warn("walk", "USN node table too large, falling back");
      ok = false;
      break;
    }
  }
  CloseHandle(h);
  if (!ok) return false;
  if (cancel && cancel->load()) return true;

  std::string vol_prefix = wide_to_utf8(mount);
  auto paths = usn_resolve_paths(nodes, vol_prefix);
  auto want = path_normalize(root);
  std::uint64_t emitted = 0;
  for (auto& [frn, full] : paths) {
    if (cancel && cancel->load()) break;
    auto norm = path_normalize(full);
    if (norm.size() < want.size()) continue;
    if (norm.compare(0, want.size(), want) != 0) continue;
    if (norm.size() > want.size() && norm[want.size()] != '\\' && norm[want.size()] != '/')
      continue;
    auto nit = nodes.find(frn);
    if (nit == nodes.end()) continue;
    WalkEntry e;
    e.path = full;
    e.st.exists = true;
    e.st.is_dir = nit->second.is_dir;
    auto slash = full.find_last_of("/\\");
    std::string nm = slash == std::string::npos ? full : full.substr(slash + 1);
    if (path_is_excluded(cfg, full, nm)) {
      if (stats) ++stats->skipped;
      continue;
    }
    try {
      on_entry(e);
    } catch (...) {
      if (stats) ++stats->errors;
    }
    if (stats) ++stats->visited;
    ++emitted;
  }
  log_info("walk", "USN scan emitted " + std::to_string(emitted) + " entries");
  return true;
}

}  // namespace wilfred
#else
namespace wilfred {
// Non-Windows builds use walk_tree; see walker.cpp.
}  // namespace wilfred
#endif
