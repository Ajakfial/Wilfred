#pragma once

#include <string>

// Minimal JSON helpers for the flat messages the wizard and host exchange.
// The host only ever parses messages produced by installer/ui, so a full
// parser would be overkill; unknown fields are ignored and malformed input
// degrades to defaults.

std::wstring JsonEscape(const std::wstring& value);

// Returns the string value for `key`, or an empty string when the key is
// missing, the value is not a string, or the JSON is malformed.
std::wstring JsonStringValue(const std::wstring& json, const wchar_t* key);

// Returns the boolean value for `key`; `fallback` when missing/not a bool.
bool JsonBoolValue(const std::wstring& json, const wchar_t* key, bool fallback);
