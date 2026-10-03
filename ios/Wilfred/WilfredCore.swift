// WilfredCore.swift — Swift wrapper around WilfredCoreBridge (ObjC++).
// Mirrors android/.../WilfredBridge.kt: same methods, same JSON parsing,
// same "never throws, empty on failure" contract. All calls are synchronous
// C++; callers dispatch off the main thread (see ContentView).

import Foundation

final class WilfredCore {
    static let shared = WilfredCore()

    private let bridge = WilfredCoreBridge()
    private(set) var ready = false

    private init() {}

    @discardableResult
    func boot(filesDir: String) -> Bool {
        if (ready) { return true }
        var err: NSString?
        ready = bridge.boot(withFilesDir: filesDir, error: &err)
        return ready
    }

    func search(_ query: String, limit: Int = 40) -> [SearchResult] {
        guard ready else { return [] }
        return CoreJSON.parseResults(bridge.searchJSON(query, limit: limit))
    }

    func assist(_ query: String) -> Assist {
        guard ready else { return Assist(correction: "", ghost: "", candidates: []) }
        return CoreJSON.parseAssist(bridge.assistJSON(query))
    }

    func actions(for index: Int) -> [ResultActionItem] {
        guard ready else { return [] }
        return CoreJSON.parseActions(bridge.actionsJSON(index))
    }

    /// Runs a C++ side-effect action (timer_stop, note_delete:*,
    /// todo_done:*, clip_*, ...). Mirrors WilfredBridge.execute.
    @discardableResult
    func execute(index: Int, actionId: String) -> Bool {
        guard ready else { return false }
        var err: NSString?
        return bridge.executeAction(index, actionId: actionId, error: &err)
    }

    func status() -> String {
        guard ready else { return "{\"ok\":false}" }
        return bridge.statusJSON()
    }

    @discardableResult
    func indexNow() -> Bool {
        guard ready else { return false }
        var err: NSString?
        return bridge.indexNow(&err)
    }

    @discardableResult
    func recordChoice(query: String, key: String) -> Bool {
        guard ready else { return false }
        return bridge.recordChoice(query, key: key)
    }

    func pushClipboard(_ text: String) {
        guard ready, !text.isEmpty else { return }
        bridge.setClipboard(text)
    }

    func preview(path: String) -> [String: Any]? {
        guard ready else { return nil }
        guard let data = bridge.previewJSON(path).data(using: .utf8) else { return nil }
        return try? JSONSerialization.jsonObject(with: data) as? [String: Any]
    }
}
