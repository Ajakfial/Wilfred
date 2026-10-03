// WilfredApp.swift — app entry point.
// iOS has no overlay-window API, so unlike Android's FloatingWService there
// is no floating W: the app icon (same W artwork) is the entry point and
// ContentView is the search UI.

import SwiftUI

@main
struct WilfredApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
