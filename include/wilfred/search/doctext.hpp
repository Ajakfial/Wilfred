#pragma once

#include <cstddef>
#include <string>

namespace wilfred {

// Extract readable text from real document formats into out_text
// (truncated to max_bytes, whitespace-collapsed):
// .docx/.xlsx/.pptx/.odt/.ods/.odp (zip+xml), .pdf (stream strings),
// .rtf (control-word stripping), .html/.htm (tag stripping).
// Returns false for unknown formats or when nothing readable is found.
bool extract_document_text(const std::string& path, std::string& out_text, std::size_t max_bytes);

// True for extensions routed through extract_document_text().
bool is_document_extension(const std::string& path);

}  // namespace wilfred
