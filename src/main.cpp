#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/plugin/registry.hpp"
#include "wilfred/search/define.hpp"
#include "wilfred/search/layouts.hpp"
#include "wilfred/search/pins.hpp"
#include "wilfred/search/setup.hpp"
#include "wilfred/service/service.hpp"
#include "wilfred/updater/updater.hpp"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
// windows.h first: shellapi.h needs its types.
#include <windows.h>

#include <shellapi.h>
#include "wilfred/core/utf8.hpp"
#endif

static void print_help() {
  std::cout << "Wilfred — cross-platform desktop search and launcher\n\n"
            << "Usage:\n"
            << "  wilfred                 Run the background daemon (overlay + indexer)\n"
            << "  wilfred daemon          Same as above\n"
            << "  wilfred search <query>  Search the local index and print results\n"
            << "  wilfred launch <query>  Search and open the top result\n"
            << "  wilfred workflow <name> [target]  Run a named workflow on a search target\n"
            << "  wilfred exec <action> [target]    Run any result action (copy_path+reveal,\n"
            << "                                    workflow:name, media:play, ...)\n"
            << "  wilfred preview <path>  Show a file preview (text head / listing)\n"
            << "  wilfred convert <src> [--to <fmt>] [--out <dst>]  Convert audio/image files\n"
            << "  wilfred bgremove <src> [--out <dst>] [--tolerance N]  Remove image background\n"
            << "  wilfred index           Scan configured roots and persist the index\n"
            << "  wilfred status          Print index statistics\n"
            << "  wilfred backup [path]   Write config/snippets/index archive\n"
            << "  wilfred restore [path]  Restore from a backup archive\n"
            << "  wilfred sync-push       Upload backup to sync.url\n"
            << "  wilfred sync-pull       Download backup from sync.url\n"
            << "  wilfred import --list   List importable launchers (Alfred, Raycast,\n"
            << "                          PowerToys Run, Flow Launcher, ...)\n"
            << "  wilfred import --detect Scan default locations for other launchers\n"
            << "  wilfred import <id|auto> [--from <path>] [--dry-run] [--overwrite]\n"
            << "                          Import hotkey, web searches, snippets, ...\n"
            << "  wilfred history-clear   Erase local search history\n"
            << "  wilfred setup [--overwrite]  First-run wizard (roots, hotkey, browser)\n"
            << "  wilfred config-validate      Validate wilfred.yml schema\n"
            << "  wilfred config-get <key>     Print one setting (section.key)\n"
            << "  wilfred config-set <key> <value>  Update one setting (validated)\n"
            << "  wilfred config-reset         Restore defaults (backs up first)\n"
            << "  wilfred config-open          Open wilfred.yml in the editor\n"
            << "  wilfred config-path          Print wilfred.yml path\n"
            << "  wilfred plugin <list|pending|install|approve|revoke>  Plugin registry + trust\n"
            << "  wilfred tile <halves|thirds|grid|columns N|rows N|stack>  Tile open windows\n"
            << "  wilfred update [--check] Check for and install updates\n"
            << "  wilfred help            Show this message\n\n"
            << "Default hotkey: Ctrl+Alt+W (Command+Option+W on macOS)\n"
            << "On Windows the daemon has no console; quit from the tray icon.\n";
}

static void print_import_help() {
  std::cout << "Usage: wilfred import [launcher] [options]\n\n"
            << "Import settings from other launchers into wilfred.yml.\n\n"
            << "Launchers (see `wilfred import --list`):\n"
            << "  alfred, raycast (macOS) | powertoys, flowlauncher, wox, keypirinha,\n"
            << "  listary (Windows) | ulauncher, albert, krunner, rofi (Linux)\n"
            << "  auto (default): import from every detected launcher.\n\n"
            << "Examples:\n"
            << "  wilfred import --list\n"
            << "  wilfred import --detect\n"
            << "  wilfred import auto --dry-run\n"
            << "  wilfred import alfred --from ~/Alfred.alfredpreferences --dry-run\n"
            << "  wilfred import flowlauncher --from Settings.json --overwrite\n"
            << "  wilfred import --from ./shortcuts.json\n\n"
            << "Options:\n"
            << "  --from <path>      File or directory to import (auto-detects format).\n"
            << "                     Without it, default locations are scanned.\n"
            << "  --dry-run, -n      Preview without writing wilfred.yml.\n"
            << "  --overwrite        Replace conflicting macros/quicklinks/snippets.\n"
            << "                     Default is merge (keep existing, add new).\n"
            << "  --no-hotkey        Skip the global hotkey.\n"
            << "  --no-searches      Skip custom web searches (macros).\n"
            << "  --no-snippets      Skip snippets.\n"
            << "  --no-aliases       Skip aliases.\n"
            << "  --no-quicklinks    Skip quicklinks.\n"
            << "  --no-theme         Skip theme.\n"
            << "  --no-browser       Skip the default search template.\n";
}

#ifdef _WIN32
static void attach_stdio_to_parent_console() {
  if (GetConsoleWindow()) return;
  if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;
  FILE* fp = nullptr;
  freopen_s(&fp, "CONOUT$", "w", stdout);
  freopen_s(&fp, "CONOUT$", "w", stderr);
  freopen_s(&fp, "CONIN$", "r", stdin);
  SetConsoleOutputCP(CP_UTF8);
}

static bool wants_console(const std::string& cmd) {
  return cmd != "daemon" && cmd != "run";
}
#endif

static int wilfred_main(int argc, char** argv) {
  try {
    wilfred::platform_init();
    std::string cmd = "daemon";
    std::string query;
    if (argc >= 2) cmd = argv[1];
#ifdef _WIN32
    if (wants_console(cmd)) attach_stdio_to_parent_console();
#endif
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
      print_help();
      return 0;
    }
    if (argc >= 3) {
      for (int i = 2; i < argc; ++i) {
        if (!query.empty()) query.push_back(' ');
        query += argv[i];
      }
    }
    wilfred::Service svc;
    if (cmd == "daemon" || cmd == "run") return svc.run_daemon();
    if (cmd == "search") {
      if (query.empty()) {
        std::cerr << "usage: wilfred search <query>\n";
        return 2;
      }
      return svc.run_search(query, 40);
    }
    if (cmd == "launch" || cmd == "open") {
      if (query.empty()) {
        std::cerr << "usage: wilfred launch <query>\n";
        return 2;
      }
      return svc.launch_by_query(query);
    }
    if (cmd == "workflow") {
      if (argc < 3) {
        std::cerr << "usage: wilfred workflow <name> [target query]\n";
        return 2;
      }
      std::string name = argv[2];
      std::string target;
      for (int i = 3; i < argc; ++i) {
        if (!target.empty()) target.push_back(' ');
        target += argv[i];
      }
      return svc.run_workflow(name, target);
    }
    if (cmd == "exec" || cmd == "action") {
      if (argc < 3) {
        std::cerr << "usage: wilfred exec <action> [target]\n";
        return 2;
      }
      std::string action = argv[2];
      std::string target;
      for (int i = 3; i < argc; ++i) {
        if (!target.empty()) target.push_back(' ');
        target += argv[i];
      }
      return svc.run_exec(action, target);
    }
    if (cmd == "index") return svc.run_index_now();
    if (cmd == "status") return svc.run_status();
    if (cmd == "setup" || cmd == "wizard" || cmd == "onboard") {
      bool overwrite = false;
      for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--overwrite" || a == "-f" || a == "--force") overwrite = true;
      }
      return wilfred::run_setup_wizard(overwrite);
    }
    if (cmd == "config-validate" || cmd == "config_check" || cmd == "validate-config" ||
        cmd == "configcheck") {
      return wilfred::run_config_validate();
    }
    if (cmd == "config-open" || cmd == "config_open" || cmd == "open-config" ||
        cmd == "config-edit") {
      return wilfred::run_config_open();
    }
    if (cmd == "config-path" || cmd == "config_path" || cmd == "configpath") {
      return wilfred::run_config_path();
    }
    if (cmd == "config-get" || cmd == "config_get" || cmd == "get-config") {
      if (argc < 3) {
        std::cerr << "usage: wilfred config-get <section.key>\n";
        return 2;
      }
      return wilfred::run_config_get(argv[2]);
    }
    if (cmd == "config-set" || cmd == "config_set" || cmd == "set-config") {
      if (argc < 4) {
        std::cerr << "usage: wilfred config-set <section.key> <value>\n";
        return 2;
      }
      std::string value = argv[3];
      for (int i = 4; i < argc; ++i) {
        value.push_back(' ');
        value += argv[i];
      }
      return wilfred::run_config_set(argv[2], value);
    }
    if (cmd == "config-reset" || cmd == "config_reset" || cmd == "reset-config") {
      return wilfred::run_config_reset();
    }
    if (cmd == "plugin" || cmd == "plugins") {
      std::string sub = argc >= 3 ? argv[2] : "list";
      std::string arg = argc >= 4 ? argv[3] : "";
      if (sub == "list" || sub == "ls") return wilfred::run_plugin_list();
      if (sub == "pending") return wilfred::run_plugin_pending();
      if (sub == "install") {
        if (arg.empty()) {
          std::cerr << "usage: wilfred plugin install <id>\n";
          return 2;
        }
        return wilfred::run_plugin_install(arg);
      }
      if (sub == "approve") {
        if (arg.empty() || arg == "--all" || arg == "all")
          return wilfred::run_plugin_approve("", true);
        return wilfred::run_plugin_approve(arg, false);
      }
      if (sub == "revoke") {
        if (arg.empty()) {
          std::cerr << "usage: wilfred plugin revoke <id>\n";
          return 2;
        }
        return wilfred::run_plugin_revoke(arg);
      }
      std::cerr
          << "usage: wilfred plugin <list|pending|install <id>|approve [id|--all]|revoke <id>>\n";
      return 2;
    }
    if (cmd == "tile" || cmd == "tiling") {
      std::string preset;
      for (int i = 2; i < argc; ++i) {
        if (!preset.empty()) preset.push_back(' ');
        preset += argv[i];
      }
      if (preset.empty()) preset = "grid";
      std::string err;
      if (!wilfred::LayoutStore::apply_tiling_preset(preset, err)) {
        std::cerr << (err.empty() ? "tiling failed" : err) << "\n";
        return 1;
      }
      std::cout << "tiled (" << preset << ")\n";
      return 0;
    }
    if (cmd == "convert") {
      std::vector<std::string> args;
      for (int i = 2; i < argc; ++i)
        args.push_back(argv[i]);
      return svc.run_convert(args);
    }
    if (cmd == "bgremove" || cmd == "bg-remove" || cmd == "removebg" || cmd == "rmbg") {
      std::vector<std::string> args;
      for (int i = 2; i < argc; ++i)
        args.push_back(argv[i]);
      return svc.run_bgremove(args);
    }
    if (cmd == "preview") {
      if (query.empty()) {
        std::cerr << "usage: wilfred preview <path>\n";
        return 2;
      }
      return svc.run_preview(query);
    }
    if (cmd == "backup") {
      bool include_index = true;
      std::string dest;
      for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--no-index")
          include_index = false;
        else
          dest = a;
      }
      return svc.run_backup(dest, include_index);
    }
    if (cmd == "restore") return svc.run_restore(query);
    if (cmd == "sync-push" || cmd == "sync_push") return svc.run_sync(true);
    if (cmd == "sync-pull" || cmd == "sync_pull") return svc.run_sync(false);
    if (cmd == "history-clear" || cmd == "clear-history") {
      wilfred::HistoryStore h;
      auto p = wilfred::default_history_path();
      h.load(p);
      h.clear();
      h.save(p);
      std::cout << "cleared " << p << "\n";
      return 0;
    }
    if (cmd == "pin" || cmd == "pin-add" || cmd == "favorite") {
      if (query.empty()) {
        std::cerr << "usage: wilfred pin <path or title>\n";
        return 2;
      }
      wilfred::PinStore::instance().configure(wilfred::default_pins_path(), {});
      wilfred::PinStore::instance().load();
      bool ok = wilfred::PinStore::instance().add(query);
      std::cout << (ok ? "pinned: " : "already pinned: ") << query << "\n";
      return ok ? 0 : 1;
    }
    if (cmd == "unpin" || cmd == "pin-remove" || cmd == "unfavorite") {
      if (query.empty()) {
        std::cerr << "usage: wilfred unpin <path or title>\n";
        return 2;
      }
      wilfred::PinStore::instance().configure(wilfred::default_pins_path(), {});
      wilfred::PinStore::instance().load();
      bool ok = wilfred::PinStore::instance().remove(query);
      std::cout << (ok ? "unpinned: " : "no pin matching: ") << query << "\n";
      return ok ? 0 : 1;
    }
    if (cmd == "pins" || cmd == "favorites") {
      wilfred::PinStore::instance().configure(wilfred::default_pins_path(), {});
      wilfred::PinStore::instance().load();
      for (auto& p : wilfred::PinStore::instance().list())
        std::cout << p << "\n";
      return 0;
    }
    if (cmd == "define" || cmd == "def" || cmd == "thesaurus" || cmd == "synonym") {
      if (query.empty()) {
        std::cerr << "usage: wilfred define <word>\n";
        return 2;
      }
      wilfred::DefineHit hit;
      if (wilfred::define_lookup(query, hit)) {
        std::cout << hit.word << " (" << hit.pos << ") — " << hit.definition << "\n";
        if (!hit.synonyms.empty()) std::cout << "synonyms: " << hit.synonyms << "\n";
        return 0;
      }
      std::cerr << "no definition for \"" << query << "\"\n";
      return 1;
    }
    if (cmd == "update" || cmd == "upgrade") {
      bool check_only = false;
      bool auto_yes = false;
      for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--check" || a == "-c")
          check_only = true;
        else if (a == "--yes" || a == "-y")
          auto_yes = true;
      }
      return wilfred::run_update_command(check_only, auto_yes);
    }
    if (cmd == "import") {
      bool list = false, detect = false, dry_run = false, overwrite = false, help = false;
      bool inc_hotkey = true, inc_searches = true, inc_snippets = true, inc_aliases = true,
           inc_quicklinks = true, inc_theme = true, inc_browser = true;
      std::string launcher, from;
      for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--list" || a == "--lists")
          list = true;
        else if (a == "--detect" || a == "--scan")
          detect = true;
        else if (a == "--dry-run" || a == "--preview" || a == "-n")
          dry_run = true;
        else if (a == "--overwrite" || a == "--force" || a == "-f")
          overwrite = true;
        else if (a == "--merge" || a == "--no-overwrite")
          overwrite = false;
        else if (a == "--no-hotkey" || a == "--no-hotkeys")
          inc_hotkey = false;
        else if (a == "--no-searches" || a == "--no-search" || a == "--no-macros")
          inc_searches = false;
        else if (a == "--no-snippets" || a == "--no-snippet")
          inc_snippets = false;
        else if (a == "--no-aliases" || a == "--no-alias")
          inc_aliases = false;
        else if (a == "--no-quicklinks" || a == "--no-quicklink")
          inc_quicklinks = false;
        else if (a == "--no-theme" || a == "--no-themes")
          inc_theme = false;
        else if (a == "--no-browser" || a == "--no-default-search")
          inc_browser = false;
        else if (a == "--from" || a == "--file" || a == "--dir" || a == "--path") {
          if (i + 1 >= argc) {
            std::cerr << "usage: wilfred import [--from <path>]\n";
            return 2;
          }
          from = argv[++i];
        } else if (a.rfind("--from=", 0) == 0)
          from = a.substr(7);
        else if (a.rfind("--file=", 0) == 0)
          from = a.substr(7);
        else if (a == "--help" || a == "-h" || a == "help")
          help = true;
        else if (a.rfind("--", 0) == 0) {
          std::cerr << "unknown option " << a << "\n";
          print_import_help();
          return 2;
        } else if (launcher.empty())
          launcher = a;
        else if (from.empty())
          from = a;  // shorthand: `wilfred import flowlauncher Settings.json`
        else {
          std::cerr << "unexpected argument " << a << "\n";
          print_import_help();
          return 2;
        }
      }
      if (help) {
        print_import_help();
        return 0;
      }
      if (list) return svc.run_import_list();
      if (detect && launcher.empty() && from.empty()) return svc.run_import_detect();
      if (launcher.empty() && from.empty() && !dry_run && !overwrite) {
        // Bare `wilfred import` is read-only discovery, never a blind write.
        return svc.run_import_detect();
      }
      if (launcher.empty()) launcher = "auto";
      return svc.run_import(launcher, from, dry_run, overwrite, inc_hotkey, inc_searches,
                            inc_snippets, inc_aliases, inc_quicklinks, inc_theme, inc_browser);
    }
    // Treat unknown first argument as a search query: `wilfred firefox`
    if (argc >= 2) {
      std::string q = argv[1];
      for (int i = 2; i < argc; ++i) {
        q.push_back(' ');
        q += argv[i];
      }
      return svc.run_search(q, 40);
    }
    return svc.run_daemon();
  } catch (const std::exception& ex) {
    wilfred::log_error("main", ex.what());
#ifdef _WIN32
    attach_stdio_to_parent_console();
#endif
    std::cerr << "wilfred: " << ex.what() << "\n";
    return 1;
  }
}

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0;
  LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!wargv) return 1;
  std::vector<std::string> storage(static_cast<std::size_t>(argc));
  std::vector<char*> argv(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    storage[static_cast<std::size_t>(i)] = wilfred::wide_to_utf8(wargv[i]);
    argv[static_cast<std::size_t>(i)] = storage[static_cast<std::size_t>(i)].data();
  }
  LocalFree(wargv);
  return wilfred_main(argc, argv.data());
}
#else
int main(int argc, char** argv) {
  return wilfred_main(argc, argv);
}
#endif
