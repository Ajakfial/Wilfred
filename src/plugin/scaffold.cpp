// `wilfred plugin new`: scaffold a gallery-ready plugin source tree.
#include "wilfred/plugin/registry.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace wilfred {
namespace fs = std::filesystem;

namespace {

bool id_char(char c, bool first) {
  if (c >= 'a' && c <= 'z') return true;
  if (c >= '0' && c <= '9') return true;
  return !first && c == '-';
}

std::string with_id(const char* tpl, const std::string& id) {
  std::string out = tpl;
  const std::string mark = "@@ID@@";
  std::string::size_type pos = 0;
  while ((pos = out.find(mark, pos)) != std::string::npos) {
    out.replace(pos, mark.size(), id);
    pos += id.size();
  }
  return out;
}

bool write_file(const fs::path& p, const std::string& text, std::string& error) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write " + p.string();
    return false;
  }
  f << text;
  f.flush();
  if (!f) {
    error = "cannot write " + p.string();
    return false;
  }
  return true;
}

const char* kManifestNative =
    "id: @@ID@@\n"
    "kind: native\n"
    "version: 1.0.0\n"
    "description: TODO: one-line description\n"
    "permissions: []\n";

const char* kManifestStdio =
    "id: @@ID@@\n"
    "kind: stdio\n"
    "version: 1.0.0\n"
    "description: TODO: one-line description\n"
    "command: python3\n"
    "args: [\"@@ID@@.py\"]\n"
    "timeout_ms: 500\n"
    "permissions: []\n";

const char* kReadme =
    "# @@ID@@\n"
    "\n"
    "TODO: what this plugin does. Gate queries on a trigger word (see the\n"
    "`dice`/`morse` sources for the pattern), then describe the trigger here.\n"
    "\n"
    "- Bare `@@ID@@` shows the usage form; Enter copies the result.\n"
    "- Offline, no permissions (declare any in `permissions:` in `plugin.yml`).\n"
    "\n"
    "@@BUILD@@\n"
    "Submit via a PR that adds `plugins/@@ID@@/` -- see `plugins/README.md` --\n"
    "and validate with `python3 scripts/pack-plugins.py --validate-only`.\n";

const char* kReadmeBuildNative =
    "## Build\n"
    "\n"
    "```bash\n"
    "cc -shared -fPIC -o @@ID@@.so @@ID@@.c          # Linux\n"
    "cc -dynamiclib -o @@ID@@.dylib @@ID@@.c         # macOS\n"
    "cl /nologo /LD @@ID@@.c /Fe@@ID@@.dll           # Windows (MSVC prompt)\n"
    "```\n"
    "\n";

const char* kReadmeBuildStdio =
    "## Run\n"
    "\n"
    "No build: point Wilfred's `plugins.directories` here (or install the\n"
    "directory as `plugins/@@ID@@/`) and approve it with\n"
    "`wilfred plugin approve @@ID@@`. Keep startup fast -- one process spawn\n"
    "per query.\n"
    "\n";

// Minimal native plugin: usage card on empty input, echo otherwise.
// Contributors replace the echo with trigger-gated results.
const char* kSourceNative =
    "// Wilfred plugin: @@ID@@ (scaffolded by `wilfred plugin new`).\n"
    "// Gate queries on a trigger word before echoing -- see dice/morse.\n"
    "// Single-file native plugin (ABI v1). Build: cc -shared -fPIC -o @@ID@@.so @@ID@@.c\n"
    "// (Windows: cl /nologo /LD @@ID@@.c /Fe@@ID@@.dll; macOS: cc -dynamiclib -o @@ID@@.dylib "
    "@@ID@@.c)\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <string.h>\n"
    "\n"
    "#define WILFRED_PLUGIN_ABI 1\n"
    "\n"
    "int wilfred_plugin_abi(void) { return WILFRED_PLUGIN_ABI; }\n"
    "const char* wilfred_plugin_id(void) { return \"@@ID@@\"; }\n"
    "\n"
    "static char g_resp[8192];\n"
    "\n"
    "static int req_q(const char* json, char* out, size_t cap) {\n"
    "  const char* k = strstr(json, \"\\\"q\\\"\");\n"
    "  if (!k) return 0;\n"
    "  k = strchr(k + 3, '\"');\n"
    "  if (!k) return 0;\n"
    "  ++k;\n"
    "  size_t n = 0;\n"
    "  while (*k && *k != '\"' && n + 1 < cap) {\n"
    "    if (*k == '\\\\' && k[1]) {\n"
    "      ++k;\n"
    "      out[n++] = (*k == 'n') ? ' ' : *k;\n"
    "      ++k;\n"
    "    } else {\n"
    "      out[n++] = *k++;\n"
    "    }\n"
    "  }\n"
    "  out[n] = '\\0';\n"
    "  return 1;\n"
    "}\n"
    "\n"
    "static void json_escape(const char* s, char* out, size_t cap) {\n"
    "  size_t n = 0;\n"
    "  for (; *s && n + 6 < cap; ++s) {\n"
    "    if (*s == '\"' || *s == '\\\\') {\n"
    "      out[n++] = '\\\\';\n"
    "      out[n++] = *s;\n"
    "    } else if ((unsigned char)*s < 0x20) {\n"
    "      n += (size_t)snprintf(out + n, cap - n, \"\\\\u%04x\", *s);\n"
    "    } else {\n"
    "      out[n++] = *s;\n"
    "    }\n"
    "  }\n"
    "  out[n] = '\\0';\n"
    "}\n"
    "\n"
    "const char* wilfred_plugin_query(const char* json_request) {\n"
    "  char q[512];\n"
    "  if (!req_q(json_request ? json_request : \"\", q, sizeof(q)) || q[0] == '\\0') {\n"
    "    snprintf(g_resp, sizeof(g_resp),\n"
    "             \"{\\\"results\\\":[{\\\"title\\\":\\\"@@ID@@ <text>\\\",\"\n"
    "             \"\\\"subtitle\\\":\\\"Describe me in README.md (Enter copies the form)\\\",\"\n"
    "             \"\\\"path\\\":\\\"@@ID@@ \\\",\\\"payload\\\":\\\"@@ID@@ \\\",\\\"score\\\":9000,"
    "\\\"action\\\":\\\"copy\\\"}]}\");\n"
    "    return g_resp;\n"
    "  }\n"
    "  char eq[1024];\n"
    "  json_escape(q, eq, sizeof(eq));\n"
    "  snprintf(g_resp, sizeof(g_resp),\n"
    "           \"{\\\"results\\\":[{\\\"title\\\":\\\"%s\\\",\\\"subtitle\\\":\\\"@@ID@@ (Enter "
    "copies)\\\",\"\n"
    "           \"\\\"path\\\":\\\"%s\\\",\\\"payload\\\":\\\"%s\\\",\\\"score\\\":500,"
    "\\\"action\\\":\\\"copy\\\"}]}\",\n"
    "           eq, eq, eq);\n"
    "  return g_resp;\n"
    "}\n"
    "\n"
    "const char* wilfred_plugin_exec(const char* json_request) {\n"
    "  (void)json_request;\n"
    "  snprintf(g_resp, sizeof(g_resp), \"{\\\"ok\\\":true}\");\n"
    "  return g_resp;\n"
    "}\n";

const char* kSourceStdio =
    "#!/usr/bin/env python3\n"
    "\"\"\"Wilfred stdio plugin: @@ID@@ (scaffolded by `wilfred plugin new`).\n"
    "Gate queries on a trigger word before echoing - see dice/morse.\n"
    "\"\"\"\n"
    "import json\n"
    "import sys\n"
    "\n"
    "\n"
    "def main():\n"
    "    try:\n"
    "        req = json.loads(sys.stdin.read() or \"{}\")\n"
    "    except Exception:\n"
    "        print(json.dumps({\"results\": []}))\n"
    "        return\n"
    "    if req.get(\"op\") == \"exec\":\n"
    "        print(json.dumps({\"ok\": True}))\n"
    "        return\n"
    "    q = req.get(\"q\", \"\")\n"
    "    if not isinstance(q, str) or not q.strip():\n"
    "        print(json.dumps({\"results\": [{\n"
    "            \"title\": \"@@ID@@ <text>\",\n"
    "            \"subtitle\": \"Describe me in README.md\",\n"
    "            \"path\": \"@@ID@@ \",\n"
    "            \"payload\": \"@@ID@@ \",\n"
    "            \"score\": 9000,\n"
    "            \"action\": \"copy\",\n"
    "        }]}))\n"
    "        return\n"
    "    print(json.dumps({\"results\": [{\n"
    "        \"title\": q,\n"
    "        \"subtitle\": \"@@ID@@ (Enter copies)\",\n"
    "        \"path\": q,\n"
    "        \"payload\": q,\n"
    "        \"score\": 500,\n"
    "        \"action\": \"copy\",\n"
    "    }]}))\n"
    "\n"
    "\n"
    "if __name__ == \"__main__\":\n"
    "    main()\n";

}  // namespace

bool plugin_scaffold_valid_id(const std::string& id) {
  if (id.empty() || id.size() > 64) return false;
  for (std::string::size_type i = 0; i < id.size(); ++i)
    if (!id_char(id[i], i == 0)) return false;
  return true;
}

bool plugin_scaffold_write(const std::string& dir, const std::string& id,
                           const std::string& kind_in, std::string& error) {
  std::string kind = kind_in;
  for (auto& c : kind) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (kind.empty()) kind = "native";
  if (!plugin_scaffold_valid_id(id)) {
    error = "bad plugin id '" + id + "' (need [a-z0-9][a-z0-9-]*)";
    return false;
  }
  if (kind != "native" && kind != "stdio") {
    error = "bad kind '" + kind_in + "' (need native or stdio)";
    return false;
  }
  std::error_code ec;
  fs::path root = dir.empty() ? fs::current_path() : fs::u8path(dir);
  fs::path target = root / fs::u8path(id);
  if (fs::exists(target, ec)) {
    error = "already exists: " + target.string();
    return false;
  }
  fs::create_directories(target, ec);
  if (ec) {
    error = "cannot create " + target.string() + ": " + ec.message();
    return false;
  }
  const bool native = (kind == "native");
  const std::string source = native ? id + ".c" : id + ".py";
  std::string readme_text = with_id(native ? kReadmeBuildNative : kReadmeBuildStdio, id);
  // kReadme has a @@BUILD@@ slot for the kind-specific section.
  std::string base = with_id(kReadme, id);
  const std::string slot = "@@BUILD@@\n";
  std::string::size_type at = base.find(slot);
  if (at != std::string::npos) base.replace(at, slot.size(), readme_text);
  if (!write_file(target / "plugin.yml", with_id(native ? kManifestNative : kManifestStdio, id),
                  error))
    return false;
  if (!write_file(target / "README.md", base, error)) return false;
  if (!write_file(target / fs::u8path(source),
                  with_id(native ? kSourceNative : kSourceStdio, id), error))
    return false;
  if (!native) {
    // Best effort: stdio scripts should be directly executable on POSIX.
    // Windows has no exec bit; failure here never fails the scaffold.
    std::error_code pec;
    fs::permissions(target / fs::u8path(source),
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add, pec);
  }
  return true;
}

int run_plugin_new(const std::string& id, const std::string& kind, const std::string& dir) {
  if (id.empty()) {
    std::cerr << "usage: wilfred plugin new <id> [--kind native|stdio] [--dir DIR]\n";
    return 2;
  }
  std::string error;
  if (!plugin_scaffold_write(dir, id, kind.empty() ? "native" : kind, error)) {
    std::cerr << error << "\n";
    return 1;
  }
  const bool native = kind.empty() || kind == "native";
  std::cout << "created " << (dir.empty() ? std::string(".") : dir) << "/" << id << "/\n"
            << "  plugin.yml, README.md, " << id << (native ? ".c" : ".py") << "\n"
            << "next: edit the trigger gate, " << (native ? "build (see README.md), " : "")
            << "then `wilfred plugin approve " << id << "`\n";
  return 0;
}

}  // namespace wilfred
