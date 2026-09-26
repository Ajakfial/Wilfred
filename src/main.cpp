#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/service/service.hpp"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include "wilfred/core/utf8.hpp"
#include <windows.h>
#include <shellapi.h>
#endif

static void print_help() {
  std::cout
      << "Wilfred — cross-platform desktop search and launcher\n\n"
      << "Usage:\n"
      << "  wilfred                 Run the background daemon (overlay + indexer)\n"
      << "  wilfred daemon          Same as above\n"
      << "  wilfred search <query>  Search the local index and print results\n"
      << "  wilfred launch <query>  Search and open the top result\n"
      << "  wilfred index           Scan configured roots and persist the index\n"
      << "  wilfred status          Print index statistics\n"
      << "  wilfred backup [path]   Write config/snippets/index archive\n"
      << "  wilfred restore [path]  Restore from a backup archive\n"
      << "  wilfred sync-push       Upload backup to sync.url\n"
      << "  wilfred sync-pull       Download backup from sync.url\n"
      << "  wilfred history-clear   Erase local search history\n"
      << "  wilfred help            Show this message\n\n"
      << "Default hotkey: Ctrl+Alt+W (Command+Option+W on macOS)\n"
      << "On Windows the daemon has no console; quit from the tray icon.\n";
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
    if (cmd == "index") return svc.run_index_now();
    if (cmd == "status") return svc.run_status();
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
int main(int argc, char** argv) { return wilfred_main(argc, argv); }
#endif
