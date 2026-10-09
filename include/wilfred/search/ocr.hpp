#pragma once

// Optional OCR text extraction for images (PNG/JPG/TIFF/BMP/WebP).
//
// Uses the `tesseract` CLI when `sources.ocr` is enabled. Cross-platform:
// shells out to `tesseract <img> stdout -l <langs>` with a timeout and
// returns false when the binary is missing or extraction fails. No link-time
// dependency on Tesseract/Leptonica.

#include <cstddef>
#include <string>

namespace wilfred {

bool is_image_extension(const std::string& path);
bool is_ocr_candidate(const std::string& path);

// Extract OCR text (whitespace-collapsed, truncated to max_bytes).
// Returns false when tesseract is unavailable or yields no readable text.
bool extract_ocr_text(const std::string& path, std::string& out_text, std::size_t max_bytes,
                      const std::string& languages = "eng");

bool tesseract_available();

}  // namespace wilfred
