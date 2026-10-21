#include "wilfred/platform/native.hpp"

#ifdef _WIN32
#include <windows.h>

#include <objbase.h>
#include <uiautomation.h>

#include <chrono>
#include <string>
#include <vector>

#include "wilfred/core/utf8.hpp"

namespace wilfred {
namespace {

template <typename T>
void release_t(T*& p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

std::string bstr_to_utf8(BSTR b) {
  if (!b) return {};
  return wide_to_utf8(std::wstring(b, SysStringLen(b)));
}

// The window that had focus when the launcher was summoned. The overlay is
// focused while searching, so reading the foreground window at query time
// would always answer "Wilfred".
HWND g_menu_target = nullptr;

int control_type(IUIAutomationElement* el) {
  CONTROLTYPEID ct = 0;
  if (!el) return 0;
  if (FAILED(el->get_CurrentControlType(&ct))) return 0;
  return static_cast<int>(ct);
}

struct RawItem {
  std::string id;
  std::string title;
  std::string path;
  std::string shortcut;
  std::string app;
  IUIAutomationElement* element{nullptr};  // owned
};

using Clock = std::chrono::steady_clock;

void walk_children(IUIAutomation* uia, IUIAutomationElement* parent, const std::string& prefix,
                   int depth, const std::string& app, std::vector<RawItem>& out,
                   Clock::time_point deadline, std::size_t& budget) {
  if (!uia || !parent || depth > 4 || budget == 0) return;
  if (Clock::now() > deadline) return;

  IUIAutomationCondition* cond = nullptr;
  if (FAILED(uia->CreateTrueCondition(&cond)) || !cond) return;
  IUIAutomationElementArray* kids = nullptr;
  const HRESULT hr = parent->FindAll(TreeScope_Children, cond, &kids);
  cond->Release();
  if (FAILED(hr) || !kids) return;

  int n = 0;
  kids->get_Length(&n);
  for (int i = 0; i < n && budget > 0; ++i) {
    if (Clock::now() > deadline) break;
    IUIAutomationElement* child = nullptr;
    if (FAILED(kids->GetElement(i, &child)) || !child) continue;
    --budget;

    BSTR name = nullptr;
    child->get_CurrentName(&name);
    std::string title = bstr_to_utf8(name);
    if (name) SysFreeString(name);

    BSTR accel = nullptr;
    child->get_CurrentAcceleratorKey(&accel);
    std::string shortcut = bstr_to_utf8(accel);
    if (accel) SysFreeString(accel);

    if (!title.empty()) {
      RawItem item;
      item.title = title;
      item.path = prefix.empty() ? title : prefix + " > " + title;
      item.shortcut = shortcut;
      item.app = app;
      item.id = item.path;
      child->AddRef();
      item.element = child;
      out.push_back(std::move(item));
    }

    const std::string next_prefix = title.empty() ? prefix : (prefix.empty() ? title : prefix + " > " + title);

    // A closed submenu reports no children. Open it just long enough to read
    // its items, then close it again; the caller has already paid for the
    // enumeration, so the flicker is bounded by the deadline.
    bool expanded = false;
    IUIAutomationExpandCollapsePattern* ec = nullptr;
    if (SUCCEEDED(child->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, IID_PPV_ARGS(&ec))) &&
        ec) {
      ExpandCollapseState state = ExpandCollapseState_Collapsed;
      if (SUCCEEDED(ec->get_CurrentExpandCollapseState(&state)) &&
          state == ExpandCollapseState_Collapsed) {
        if (SUCCEEDED(ec->Expand())) {
          expanded = true;
          Sleep(20);
        }
      }
    }

    walk_children(uia, child, next_prefix, depth + 1, app, out, deadline, budget);

    if (expanded && ec) ec->Collapse();
    release_t(ec);
    child->Release();
  }
  kids->Release();
}

// Returns the top-level window element that owns the focused control, or null.
IUIAutomationElement* focused_window(IUIAutomation* uia) {
  IUIAutomationElement* focused = nullptr;
  if (FAILED(uia->GetFocusedElement(&focused)) || !focused) return nullptr;
  IUIAutomationTreeWalker* walker = nullptr;
  if (FAILED(uia->get_ControlViewWalker(&walker)) || !walker) {
    focused->Release();
    return nullptr;
  }
  IUIAutomationElement* cur = focused;
  for (int hops = 0; cur && hops < 24; ++hops) {
    if (control_type(cur) == UIA_WindowControlTypeId) {
      walker->Release();
      return cur;
    }
    IUIAutomationElement* parent = nullptr;
    if (FAILED(walker->GetParentElement(cur, &parent))) parent = nullptr;
    cur->Release();
    cur = parent;
  }
  if (cur) cur->Release();
  walker->Release();
  return nullptr;
}

bool collect(std::vector<RawItem>& out, std::string& error) {
  const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool owns_com = SUCCEEDED(init);

  IUIAutomation* uia = nullptr;
  if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&uia))) ||
      !uia) {
    if (owns_com) CoUninitialize();
    error = "UI Automation is not available";
    return false;
  }

  bool ok = false;
  IUIAutomationElement* win = nullptr;
  if (g_menu_target && IsWindow(g_menu_target)) {
    uia->ElementFromHandle(g_menu_target, &win);
  }
  if (!win) win = focused_window(uia);
  if (!win) {
    error = "no focused window";
  } else {
    BSTR wn = nullptr;
    win->get_CurrentName(&wn);
    const std::string app = bstr_to_utf8(wn);
    if (wn) SysFreeString(wn);

    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    v.lVal = UIA_MenuBarControlTypeId;
    IUIAutomationCondition* cond = nullptr;
    IUIAutomationElement* bar = nullptr;
    if (SUCCEEDED(uia->CreatePropertyCondition(UIA_ControlTypePropertyId, v, &cond)) && cond) {
      win->FindFirst(TreeScope_Descendants, cond, &bar);
      cond->Release();
    }
    if (!bar) {
      error = "this app does not expose a menu bar";
    } else {
      std::size_t budget = 200;
      walk_children(uia, bar, {}, 0, app, out, Clock::now() + std::chrono::milliseconds(400),
                    budget);
      bar->Release();
      ok = !out.empty();
      if (!ok) error = "the menu bar is empty";
    }
    win->Release();
  }

  release_t(uia);
  if (owns_com) CoUninitialize();
  return ok;
}

void release_items(std::vector<RawItem>& items) {
  for (auto& it : items) release_t(it.element);
  items.clear();
}

}  // namespace

void native_menu_remember_foreground() {
  const HWND fg = GetForegroundWindow();
  if (fg && IsWindow(fg)) g_menu_target = fg;
}

bool native_active_menu_items(std::vector<NativeMenuItem>& out, std::string& error) {
  error.clear();
  std::vector<RawItem> raw;
  if (!collect(raw, error)) {
    release_items(raw);
    return false;
  }
  for (const auto& r : raw) {
    NativeMenuItem item;
    item.id = r.id;
    item.title = r.title;
    item.path = r.path;
    item.shortcut = r.shortcut;
    item.app = r.app;
    out.push_back(std::move(item));
  }
  release_items(raw);
  return true;
}

bool native_invoke_menu_item(const std::string& id, std::string& error) {
  error.clear();
  std::vector<RawItem> raw;
  if (!collect(raw, error)) {
    release_items(raw);
    return false;
  }
  bool ok = false;
  for (auto& r : raw) {
    if (r.id != id || !r.element) continue;
    IUIAutomationInvokePattern* inv = nullptr;
    if (SUCCEEDED(r.element->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&inv))) &&
        inv) {
      ok = SUCCEEDED(inv->Invoke());
      release_t(inv);
    }
    if (!ok) {
      IUIAutomationExpandCollapsePattern* ec = nullptr;
      if (SUCCEEDED(
              r.element->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, IID_PPV_ARGS(&ec))) &&
          ec) {
        ok = SUCCEEDED(ec->Expand());
        release_t(ec);
      }
    }
    break;
  }
  if (!ok && error.empty()) error = "menu item is gone (did the app change?)";
  release_items(raw);
  return ok;
}

}  // namespace wilfred
#else
namespace wilfred {
bool native_active_menu_items(std::vector<NativeMenuItem>&, std::string&);
bool native_invoke_menu_item(const std::string&, std::string&);
void native_menu_remember_foreground();
}  // namespace wilfred
#endif
