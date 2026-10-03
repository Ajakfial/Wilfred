// CoreTypes.swift — Decodable models for the C++ bridge JSON.
// Mirrors android/.../WilfredBridge.kt (SearchResult, ResultActionItem,
// Assist) field for field, including the per-result `actions` array that
// powers the long-press menu.

import Foundation

struct ResultActionItem: Identifiable, Hashable {
    let id: String
    let label: String
}

struct SearchResult: Identifiable, Hashable {
    let index: Int
    let title: String
    let subtitle: String
    let path: String
    let payload: String
    let score: Int
    let action: String
    let category: String
    let kind: String
    let actions: [ResultActionItem]

    var id: Int { index }

    var displayTitle: String {
        if (!title.isEmpty) return title
        if (!path.isEmpty) return path
        return payload
    }

    var displaySubtitle: String {
        if (!subtitle.isEmpty) return subtitle
        if (!path.isEmpty) return path
        return payload
    }

    var badge: String {
        if (!category.isEmpty) return category
        if (!action.isEmpty) return action
        return kind
    }
}

struct Assist {
    let correction: String
    let ghost: String
    let candidates: [String]
}

enum CoreJSON {
    static func parseResults(_ json: String) -> [SearchResult] {
        guard let data = json.data(using: .utf8),
              let arr = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]] else {
            return []
        }
        var out: [SearchResult] = []
        out.reserveCapacity(arr.count)
        for (i, o) in arr.enumerated() {
            var actions: [ResultActionItem] = []
            if let ja = o["actions"] as? [[String: Any]] {
                for a in ja {
                    guard let aid = a["id"] as? String, !aid.isEmpty else { continue }
                    actions.append(ResultActionItem(id: aid, label: a["label"] as? String ?? aid))
                }
            }
            out.append(SearchResult(
                index: i,
                title: o["title"] as? String ?? "",
                subtitle: o["subtitle"] as? String ?? "",
                path: o["path"] as? String ?? "",
                payload: o["payload"] as? String ?? "",
                score: o["score"] as? Int ?? 0,
                action: o["action"] as? String ?? "",
                category: o["category"] as? String ?? "",
                kind: o["kind"] as? String ?? "",
                actions: actions
            ))
        }
        return out
    }

    static func parseAssist(_ json: String) -> Assist {
        guard let data = json.data(using: .utf8),
              let o = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return Assist(correction: "", ghost: "", candidates: [])
        }
        return Assist(
            correction: o["correction"] as? String ?? "",
            ghost: o["ghost"] as? String ?? "",
            candidates: o["candidates"] as? [String] ?? []
        )
    }

    static func parseActions(_ json: String) -> [ResultActionItem] {
        guard let data = json.data(using: .utf8),
              let arr = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]] else {
            return []
        }
        var out: [ResultActionItem] = []
        for a in arr {
            guard let aid = a["id"] as? String, !aid.isEmpty else { continue }
            out.append(ResultActionItem(id: aid, label: a["label"] as? String ?? aid))
        }
        return out
    }
}
