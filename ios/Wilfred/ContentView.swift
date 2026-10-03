// ContentView.swift — the Wilfred search UI.
// Mirrors android/.../MainActivity.kt + res/layout/activity_main.xml, top to
// bottom: search field, did-you-mean bar, ghost completion, candidate strip,
// status line, results list. All search logic runs in C++ (WilfredCore);
// this view only owns the search bar, assist strip, results and iOS opens.

import Foundation
import SwiftUI
import UIKit
import Combine

final class SearchModel: ObservableObject {
    @Published var query = ""
    @Published var results: [SearchResult] = []
    @Published var assist = Assist(correction: "", ghost: "", candidates: [])
    @Published var status = "Starting Wilfred core…"
    @Published var booted = false
    @Published var notice: String? = nil

    private var seq = 0
    private let queue = DispatchQueue(label: "com.wilfred.launcher.search", qos: .userInitiated)

    var lastQuery = ""

    func boot() {
        queue.async { [weak self] in
            guard let strongSelf = self else { return }
            let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)
                .first?.path ?? NSTemporaryDirectory()
            let ok = WilfredCore.shared.boot(filesDir: docs)
            let clip = WilfredActions.pasteboardText()
            if (!clip.isEmpty) { WilfredCore.shared.pushClipboard(clip) }
            DispatchQueue.main.async {
                strongSelf.booted = ok
                if (!ok) {
                    strongSelf.status = "Could not start Wilfred core."
                } else {
                    strongSelf.status = strongSelf.prettyStatus(WilfredCore.shared.status())
                    strongSelf.runSearch("")
                }
            }
        }
    }

    func refreshClipboard() {
        queue.async {
            let clip = WilfredActions.pasteboardText()
            if (!clip.isEmpty) { WilfredCore.shared.pushClipboard(clip) }
        }
    }

    func runSearch(_ q: String) {
        lastQuery = q
        guard booted else { return }
        seq += 1
        let my = seq
        queue.async { [weak self] in
            guard let strongSelf = self else { return }
            let results = WilfredCore.shared.search(q)
            let assist = WilfredCore.shared.assist(q)
            let statusJSON = q.isEmpty ? WilfredCore.shared.status() : ""
            DispatchQueue.main.async {
                guard my == strongSelf.seq, q == strongSelf.query else { return }
                strongSelf.results = results
                strongSelf.assist = assist
                if (q.isEmpty) {
                    strongSelf.status = strongSelf.prettyStatus(statusJSON)
                } else if (results.isEmpty) {
                    strongSelf.status = "No results"
                } else if (results.count == 1) {
                    strongSelf.status = "1 result"
                } else {
                    strongSelf.status = "\(results.count) results"
                }
            }
        }
    }

    func refresh() { runSearch(query) }

    func reindex(notice: @escaping (String) -> Void) {
        queue.async { [weak self] in
            let ok = WilfredCore.shared.indexNow()
            DispatchQueue.main.async {
                notice(ok ? "Index updated" : "Index failed")
                self?.refresh()
            }
        }
    }

    func prettyStatus(_ json: String) -> String {
        guard let data = json.data(using: .utf8),
              let o = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              (o["ok"] as? Bool) == true else {
            return "Could not start Wilfred core."
        }
        let r = o["records"] as? Int ?? 0
        let a = o["apps"] as? Int ?? 0
        return "\(r) files · \(a) apps — C++ core ready"
    }
}

private struct PreviewSheet: Identifiable {
    let title: String
    let message: String
    var id: String { title + "\n" + message }
}

struct ContentView: View {
    @StateObject private var model = SearchModel()
    @FocusState private var focused: Bool
    @State private var menuResult: SearchResult? = nil
    @State private var preview: PreviewSheet? = nil
    @State private var shareURL: URL? = nil

    private var sharing: Binding<Bool> {
        Binding(get: { shareURL != nil }, set: { if (!$0) { shareURL = nil } })
    }

    var body: some View {
        NavigationStack {
            VStack(alignment: .leading, spacing: 0) {
                // Search bar (mirrors R.id.search_input).
                TextField("Search files, apps, web…", text: $model.query)
                    .textFieldStyle(.roundedBorder)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                    .focused($focused)
                    .padding(.horizontal, 12)
                    .padding(.top, 12)
                    .onChange(of: model.query) { _, q in model.runSearch(q) }
                    .onSubmit {
                        if let top = model.results.first {
                            tap(top)
                        }
                    }

                // Did-you-mean bar (mirrors R.id.correction_bar).
                if (!model.query.isEmpty && !model.assist.correction.isEmpty) {
                    Button("Did you mean: \(model.assist.correction)?") {
                        model.query = model.assist.correction
                    }
                    .foregroundStyle(.tint)
                    .padding(.horizontal, 12)
                    .padding(.top, 6)
                }

                // Ghost completion (mirrors R.id.ghost_line).
                if (!model.query.isEmpty && !model.assist.ghost.isEmpty &&
                    model.assist.ghost.lowercased() != model.query.lowercased()) {
                    Button(model.assist.ghost) {
                        model.query = model.assist.ghost
                    }
                    .foregroundStyle(.secondary)
                    .font(.subheadline)
                    .padding(.horizontal, 12)
                    .padding(.top, 2)
                }

                // Candidate strip (mirrors R.id.candidates_row).
                if (!model.assist.candidates.isEmpty) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack {
                            ForEach(model.assist.candidates.prefix(6), id: \.self) { c in
                                Button(c) { model.query = c }
                                    .buttonStyle(.bordered)
                            }
                        }
                        .padding(.horizontal, 12)
                    }
                    .padding(.top, 4)
                }

                // Status line (mirrors R.id.status_line).
                Text(model.status)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 12)
                    .padding(.top, 6)

                // Results list (mirrors R.id.result_list).
                List(model.results) { r in
                    ResultRow(result: r)
                        .contentShape(Rectangle())
                        .onTapGesture { tap(r) }
                        .onLongPressGesture { menuResult = r }
                        .contextMenu {
                            ForEach(WilfredActions.menuItems(for: r, index: r.index)) { a in
                                Button(a.label) { act(r, a.id) }
                            }
                            Button("Preview") { showPreview(r) }
                        }
                        .swipeActions(edge: .trailing) {
                            Button("Preview") { showPreview(r) }
                        }
                }
                .listStyle(.plain)
            }
            .navigationTitle("Wilfred")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    Menu {
                        Button("Reindex") {
                            model.reindex { n in model.notice = n }
                        }
                        Button("Status") {
                            model.status = model.prettyStatus(WilfredCore.shared.status())
                        }
                    } label: {
                        Image(systemName: "ellipsis.circle")
                    }
                }
            }
            .alert(item: $preview) { p in
                Alert(title: Text(p.title), message: Text(p.message),
                      dismissButton: .default(Text("OK")))
            }
            .confirmationDialog(
                menuResult?.displayTitle ?? "",
                isPresented: Binding(get: { menuResult != nil },
                                     set: { if (!$0) { menuResult = nil } }),
                titleVisibility: .visible
            ) {
                if let r = menuResult {
                    ForEach(WilfredActions.menuItems(for: r, index: r.index)) { a in
                        Button(a.label) { act(r, a.id) }
                    }
                    Button("Preview") { showPreview(r) }
                    Button("Cancel", role: .cancel) { menuResult = nil }
                }
            }
            .sheet(isPresented: sharing) {
                if let url = shareURL { ShareSheet(url: url) }
            }
            .onAppear {
                model.boot()
                focused = true
            }
            .onReceive(NotificationCenter.default.publisher(
                for: UIApplication.willEnterForegroundNotification)) { _ in
                model.refreshClipboard()
                focused = true
            }
            .toast(text: $model.notice)
        }
    }

    private func showNotice(_ s: String) { model.notice = s }

    private func tap(_ r: SearchResult) {
        WilfredActions.openResult(r, query: model.lastQuery, index: r.index,
                                  notice: showNotice,
                                  shareFile: { shareURL = $0 },
                                  refresh: { model.refresh() })
    }

    private func act(_ r: SearchResult, _ id: String) {
        WilfredActions.runAction(id, on: r, index: r.index,
                                 notice: showNotice,
                                 shareFile: { shareURL = $0 },
                                 refresh: { model.refresh() })
    }

    private func showPreview(_ r: SearchResult) {
        let p = WilfredActions.previewContent(for: r)
        preview = PreviewSheet(title: p.title, message: p.message)
    }
}

// MARK: - Share sheet (UIActivityViewController in SwiftUI)

struct ShareSheet: UIViewControllerRepresentable {
    let url: URL
    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: [url], applicationActivities: nil)
    }
    func updateUIViewController(_ vc: UIActivityViewController, context: Context) {}
}

// MARK: - Lightweight notice banner (Android Toast equivalent)

private struct ToastModifier: ViewModifier {
    @Binding var text: String?
    func body(content: Content) -> some View {
        content.overlay(alignment: .bottom) {
            if let text {
                Text(text)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 10)
                    .background(.ultraThinMaterial, in: Capsule())
                    .padding(.bottom, 24)
                    .transition(.opacity)
                    .onAppear {
                        DispatchQueue.main.asyncAfter(deadline: .now() + 1.6) {
                            if (self.text == text) { self.text = nil }
                        }
                    }
            }
        }
        .animation(.easeInOut, value: text)
    }
}

extension View {
    func toast(text: Binding<String?>) -> some View {
        modifier(ToastModifier(text: text))
    }
}
