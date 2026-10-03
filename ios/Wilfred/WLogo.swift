// WLogo.swift — the Wilfred "W" badge drawn in SwiftUI.
// Same artwork as android/.../res/drawable/ic_w.xml: periwinkle
// rounded square (#6C8CFF) with a dark rounded "W" stroke, 56pt space.

import SwiftUI

struct WLogo: View {
    var size: CGFloat = 56

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: size * 10 / 56, style: .continuous)
                .fill(Color(red: 0x6C / 255.0, green: 0x8C / 255.0, blue: 0xFF / 255.0))
            Path { p in
                // M14,18 L20,38 L28,24 L36,38 L42,18 (Android viewport 56).
                let s = size / 56
                p.move(to: CGPoint(x: 14 * s, y: 18 * s))
                p.addLine(to: CGPoint(x: 20 * s, y: 38 * s))
                p.addLine(to: CGPoint(x: 28 * s, y: 24 * s))
                p.addLine(to: CGPoint(x: 36 * s, y: 38 * s))
                p.addLine(to: CGPoint(x: 42 * s, y: 18 * s))
            }
            .stroke(Color(red: 0x0D / 255.0, green: 0x0E / 255.0, blue: 0x13 / 255.0),
                    style: StrokeStyle(lineWidth: max(2, size * 4 / 56),
                                        lineCap: .round, lineJoin: .round))
        }
        .frame(width: size, height: size)
        .accessibilityLabel("Open Wilfred search")
    }
}
