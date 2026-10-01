#pragma once

// Calendar, contacts, and notes search backends.
//
// All three are file-first and cross-platform:
//  - Calendar: parses .ics (VEVENT: SUMMARY/DTSTART/DESCRIPTION) under
//    configured paths + platform defaults (Thunderbird/Lightning, Evolution,
//    Apple Calendars, Outlook exports).
//  - Contacts: parses .vcf (FN/N/EMAIL/TEL) under configured paths +
//    platform defaults.
//  - Notes: indexes Markdown/text notes (.md/.txt/.org) under configured
//    paths + platform defaults (~/Notes, Nextcloud, Obsidian vaults).
//
// Each provider caches for 60s and returns SearchResults with categories
// `calendar`, `contact`, `note` so the overlay can badge them.

#include "wilfred/providers/provider.hpp"

#include <string>
#include <vector>

namespace wilfred {

std::vector<std::string> default_calendar_roots();
std::vector<std::string> default_contacts_roots();
std::vector<std::string> default_notes_roots();

class CalendarProvider : public SearchProvider {
 public:
  std::string id() const override { return "calendar"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

class ContactsProvider : public SearchProvider {
 public:
  std::string id() const override { return "contacts"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

class NotesProvider : public SearchProvider {
 public:
  std::string id() const override { return "notes"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

}  // namespace wilfred
