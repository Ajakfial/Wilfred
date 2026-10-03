// ResultRow.swift — one search result row.
// Mirrors android/.../ResultAdapter.kt + res/layout/row_result.xml:
// title, subtitle, trailing badge (category → action → kind).

import SwiftUI

struct ResultRow: View {
    let result: SearchResult

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(result.displayTitle)
                    .font(.body)
                    .lineLimit(2)
                if (!result.displaySubtitle.isEmpty &&
                    result.displaySubtitle != result.displayTitle) {
                    Text(result.displaySubtitle)
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                        .lineLimit(2)
                }
            }
            Spacer()
            if (!result.badge.isEmpty) {
                Text(result.badge)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, 10)
    }
}
