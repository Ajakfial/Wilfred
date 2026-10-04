// WilfredActions.swift — shared result-opening logic for ContentView.
// Mirrors android/.../WilfredActions.kt: all ranking/search/calc/snippets/
// notes/todos/timers logic lives in C++ (wilfred_core via WilfredCore); this
// layer only performs iOS opens, UIPasteboard writes and the share sheet.
//
// Differences from Android (sandbox-enforced, documented in docs/ios.md):
// - No app launching: iOS exposes no installed-app list, so there are no
//   `package:` results and register_app is never called.
// - No file-manager reveal: files open in a share sheet
//   (UIActivityViewController) instead.
// - Window/system/screenshot/media/process cards report "desktop-only".

import Foundation
import UIKit

enum WilfredActions {
    static func copyText(_ text: String, notice: (String) -> Void) {
        guard !text.isEmpty else { return }
        UIPasteboard.general.string = text
        WilfredCore.shared.pushClipboard(text)
        notice("Copied")
    }

    static func pasteboardText() -> String {
        UIPasteboard.general.string ?? ""
    }

    /// Tap (primary) action. `index` is the position in the last search.
    static func openResult(_ r: SearchResult, query: String, index: Int,
                           notice: @escaping (String) -> Void,
                           shareFile: @escaping (URL) -> Void,
                           refresh: @escaping () -> Void) {
        WilfredCore.shared.recordChoice(query: query,
                                        key: r.path.isEmpty ? r.payload : r.path)
        // Toggles / settings / config / approvals: iOS owns Settings and the
        // volume/brightness UI, so handle natively instead of the C++ desktop
        // backends (stubs on iOS). Returns true when handled.
        if (handleMobileSystem(r, index: index, notice: notice, refresh: refresh)) { return }
        // Calculator / converter / generic copy cards: copy payload.
        if (r.action == "calc" || r.action == "convert" || r.action == "copy" ||
            (r.action == "mini" && copyableMini(r))) {
            WilfredCore.shared.execute(index: index, actionId: "copy_text")
            copyText(r.payload.isEmpty ? r.title : r.payload, notice: notice)
            refresh()
            return
        }
        // Snippet expand: paste = copy body.
        if (r.action == "expand" || r.category == "snippet") {
            WilfredCore.shared.execute(index: index, actionId: "paste")
            copyText(r.payload.isEmpty ? r.title : r.payload, notice: notice)
            return
        }
        // Web results.
        if (r.action == "web" || r.path.hasPrefix("http://") || r.path.hasPrefix("https://") ||
            r.payload.hasPrefix("http://") || r.payload.hasPrefix("https://")) {
            let raw = r.path.hasPrefix("http") ? r.path : r.payload
            if let url = URL(string: raw), !raw.isEmpty {
                UIApplication.shared.open(url)
            } else {
                copyText(r.title, notice: notice)
            }
            return
        }
        // Desktop-only cards: report clearly instead of failing silently.
        if (r.action == "window" || r.category == "window" ||
            r.action == "system" || r.category == "system" ||
            r.action == "screenshot" || r.category == "screenshot") {
            if (r.action == "window" || r.category == "window") {
                notice("Window management is desktop-only")
            } else if (r.action == "system" || r.category == "system") {
                notice("System power actions are unavailable on iOS")
            } else {
                notice("Screenshots use the system gesture on iOS")
            }
            return
        }
        // Files / folders inside the sandbox open in the share sheet.
        if (!r.path.isEmpty) {
            let url = URL(fileURLWithPath: r.path)
            if (FileManager.default.fileExists(atPath: r.path)) {
                shareFile(url)
                return
            }
        }
        // Fallback: copy whatever text the card carries.
        let text = !r.payload.isEmpty ? r.payload : (!r.path.isEmpty ? r.path : r.title)
        copyText(text, notice: notice)
        refresh()
    }

    /// Native iOS handling for toggle/settings/config/plugin/layout cards.
    /// System radios, volume, and brightness are owned by iOS: toggles open
    /// the Settings app (the only third-party entry point Apple allows),
    /// config/setup/plugin payloads run through C++ (mobile wilfred.yml and
    /// trust store), and window/tiling cards report desktop-only.
    static func handleMobileSystem(_ r: SearchResult, index: Int,
                                   notice: @escaping (String) -> Void,
                                   refresh: @escaping () -> Void) -> Bool {
        let payload = r.payload.isEmpty ? r.path : r.payload
        if (r.category == "toggle" || payload.hasPrefix("toggle:")) {
            if (payload.hasPrefix("toggle:volume:") || payload.hasPrefix("toggle:brightness:")) {
                notice("Use the side buttons / Control Center on iOS")
            } else {
                openSettingsApp(notice: notice)
            }
            refresh()
            return true
        }
        if (r.category == "settings" || payload.hasPrefix("settings:")) {
            openSettingsApp(notice: notice)
            refresh()
            return true
        }
        if (r.category == "config" || r.category == "setup" ||
            payload.hasPrefix("config:") || payload.hasPrefix("setup:")) {
            let ok = WilfredCore.shared.execute(index: index, actionId: "open")
            notice(ok ? "Done" : "Action failed")
            refresh()
            return true
        }
        if (r.category == "plugins" || payload.hasPrefix("plugin_approve")) {
            let ok = WilfredCore.shared.execute(index: index, actionId: "open")
            notice(ok ? "Done" : "Action failed")
            refresh()
            return true
        }
        if (r.category == "layout" || payload.hasPrefix("tile:") ||
            payload.hasPrefix("layout_apply:")) {
            notice("Window management is desktop-only")
            return true
        }
        return false
    }

    static func openSettingsApp(notice: @escaping (String) -> Void) {
        if let url = URL(string: UIApplication.openSettingsURLString),
           UIApplication.shared.canOpenURL(url) {
            UIApplication.shared.open(url)
        } else {
            notice("Open the Settings app to change this")
        }
    }

    static func copyableMini(_ r: SearchResult) -> Bool {
        r.category == "timer" || r.category == "stopwatch" || r.category == "note" ||
        r.category == "todo" || r.category == "clips" || r.category == "clipboard" ||
        r.category == "process" || r.category == "kill" || r.category == "media" ||
        r.category == "color" || r.kind == "uuid" || r.kind == "base64" ||
        r.kind == "sha256" || r.kind == "dev"
    }

    /// Long-press menu items: full C++ action list, falling back to Open/Copy.
    static func menuItems(for r: SearchResult, index: Int) -> [ResultActionItem] {
        let items = WilfredCore.shared.actions(for: index)
        if (!items.isEmpty) { return items }
        return [ResultActionItem(id: "open", label: "Open"),
                ResultActionItem(id: "copy_text", label: "Copy")]
    }

    static func runAction(_ actionId: String, on r: SearchResult, index: Int,
                          notice: @escaping (String) -> Void,
                          shareFile: @escaping (URL) -> Void,
                          refresh: @escaping () -> Void) {
        let core = WilfredCore.shared
        if (actionId == "copy_text" || actionId == "copy" || actionId == "paste" ||
            actionId == "expand" || actionId == "copy_path" || actionId == "copy_name" ||
            actionId == "copy_file_uri" || actionId == "copy_posix" ||
            actionId == "copy_wsl" || actionId == "hash_file") {
            let ok = core.execute(index: index, actionId: actionId)
            let text: String
            if (actionId == "copy_path" || actionId == "copy_file_uri") {
                text = r.path.isEmpty ? r.payload : r.path
            } else if (actionId == "copy_name") {
                text = r.title
            } else {
                text = r.payload.isEmpty ? r.title : r.payload
            }
            if (ok || !text.isEmpty) { copyText(text, notice: notice) }
            refresh()
            return
        }
        if (actionId == "open" || actionId.hasPrefix("open_with:")) {
            openResult(r, query: "", index: index, notice: notice, shareFile: shareFile, refresh: refresh)
            return
        }
        if (actionId == "reveal") {
            // iOS has no file-manager reveal; show the containing folder.
            notice((r.path as NSString).deletingLastPathComponent)
            return
        }
        if (actionId == "timer_stop" || actionId.hasPrefix("note_delete:") ||
            actionId.hasPrefix("todo_done:") || actionId.hasPrefix("todo_undo:") ||
            actionId.hasPrefix("todo_delete:") || actionId == "clip_pin" ||
            actionId == "clip_unpin" || actionId == "clip_clear" ||
            actionId.hasPrefix("layout_apply:") || actionId.hasPrefix("workflow:")) {
            notice(core.execute(index: index, actionId: actionId) ? "Done" : "Action failed")
            refresh()
            return
        }
        if (actionId.hasPrefix("media:") || actionId == "kill_process" ||
            actionId.hasPrefix("window_") || actionId.hasPrefix("tile:") ||
            actionId.hasPrefix("layout_apply:") || actionId == "transcribe_run" ||
            actionId.hasPrefix("dictate_run") || actionId.hasPrefix("focus_window:")) {
            notice("Not available on iOS (desktop-only)")
            return
        }
        if (!core.execute(index: index, actionId: actionId)) {
            openResult(r, query: "", index: index, notice: notice, shareFile: shareFile, refresh: refresh)
        } else {
            refresh()
        }
    }

    /// F3-style peek data: file/folder info + text head from preview_json.
    static func previewContent(for r: SearchResult) -> (title: String, message: String) {
        let target = r.path.isEmpty ? r.payload : r.path
        if (target.isEmpty || target.hasPrefix("http")) {
            return (r.title, r.subtitle.isEmpty ? r.payload : r.subtitle)
        }
        guard let obj = WilfredCore.shared.preview(path: target),
              (obj["exists"] as? Bool) == true else {
            return (r.title.isEmpty ? target : r.title, "File no longer exists.")
        }
        let isDir = obj["is_dir"] as? Bool ?? false
        let size = obj["size"] as? Int ?? 0
        let preview = obj["preview"] as? String ?? ""
        var head = isDir ? "Folder contents:\n\(preview)" : "Size: \(size) bytes\n\n\(preview)"
        if (!r.subtitle.isEmpty) { head = "\(r.subtitle)\n\n\(head)" }
        if (head.isEmpty) { head = "No preview available." }
        let name = obj["name"] as? String ?? target
        return (r.title.isEmpty ? name : r.title, head)
    }
}
