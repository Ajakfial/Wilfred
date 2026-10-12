// Wilfred plugin: roman numerals (`roman 2026` encodes, `roman MMXXVI` decodes).
// Single-file native plugin (ABI v1). Build: cc -shared -fPIC -o roman.so roman.c
// (Windows: cl /nologo /LD roman.c /Feroman.dll; macOS: cc -dynamiclib -o roman.dylib roman.c)
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WILFRED_PLUGIN_ABI 1

#define ROMAN_MIN 1
#define ROMAN_MAX 3999

int wilfred_plugin_abi(void) { return WILFRED_PLUGIN_ABI; }
const char* wilfred_plugin_id(void) { return "roman"; }

static char g_resp[8192];

typedef struct {
  int value;
  const char* digits;
} RomanRow;

static const RomanRow kTable[] = {
    {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"},
    {50, "L"},   {40, "XL"},  {10, "X"},  {9, "IX"},   {5, "V"},   {4, "IV"},
    {1, "I"},
};

static int roman_value(char c) {
  switch (c) {
    case 'I': return 1;
    case 'V': return 5;
    case 'X': return 10;
    case 'L': return 50;
    case 'C': return 100;
    case 'D': return 500;
    case 'M': return 1000;
    default: return 0;
  }
}

// Encode n (1-3999) into out. Returns 1 on success.
static int encode_roman(int n, char* out, size_t cap) {
  if (n < ROMAN_MIN || n > ROMAN_MAX || cap == 0) return 0;
  size_t len = 0;
  for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i) {
    while (n >= kTable[i].value) {
      size_t L = strlen(kTable[i].digits);
      if (len + L + 1 > cap) return 0;
      memcpy(out + len, kTable[i].digits, L);
      len += L;
      n -= kTable[i].value;
    }
  }
  out[len] = '\0';
  return 1;
}

// Parse a run of I/V/X/L/C/D/M (already uppercased, no spaces).
// Returns the value, or 0 when empty, illegal, or out of range.
static int parse_roman(const char* s) {
  if (!s || !*s) return 0;
  long total = 0;
  for (const char* p = s; *p; ++p) {
    int v = roman_value(*p);
    if (v == 0) return 0;
    int next = p[1] ? roman_value(p[1]) : 0;
    if (v < next)
      total -= v;
    else
      total += v;
  }
  if (total < ROMAN_MIN || total > ROMAN_MAX) return 0;
  return (int)total;
}

static int req_q(const char* json, char* out, size_t cap) {
  const char* k = strstr(json, "\"q\"");
  if (!k) return 0;
  k = strchr(k + 3, '"');
  if (!k) return 0;
  ++k;
  size_t n = 0;
  while (*k && *k != '"' && n + 1 < cap) {
    if (*k == '\\' && k[1]) {
      ++k;
      out[n++] = (*k == 'n') ? ' ' : *k;
      ++k;
    } else {
      out[n++] = *k++;
    }
  }
  out[n] = '\0';
  return 1;
}

static void json_escape(const char* s, char* out, size_t cap) {
  size_t n = 0;
  for (; *s && n + 6 < cap; ++s) {
    if (*s == '"' || *s == '\\') {
      out[n++] = '\\';
      out[n++] = *s;
    } else if ((unsigned char)*s < 0x20) {
      n += (size_t)snprintf(out + n, cap - n, "\\u%04x", *s);
    } else {
      out[n++] = *s;
    }
  }
  out[n] = '\0';
}

static int starts_roman(const char* q, const char** rest) {
  while (*q == ' ' || *q == '\t') ++q;
  if ((q[0] == 'r' || q[0] == 'R') && (q[1] == 'o' || q[1] == 'O') &&
      (q[2] == 'm' || q[2] == 'M') && (q[3] == 'a' || q[3] == 'A') &&
      (q[4] == 'n' || q[4] == 'N') &&
      (q[5] == ' ' || q[5] == '\t' || q[5] == '\0')) {
    *rest = q[5] ? q + 6 : q + 5;
    return 1;
  }
  return 0;
}

const char* wilfred_plugin_query(const char* json_request) {
  char q[512];
  if (!req_q(json_request ? json_request : "", q, sizeof(q))) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  const char* rest = NULL;
  if (!starts_roman(q, &rest)) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  while (*rest == ' ' || *rest == '\t') ++rest;
  if (*rest == '\0') {
    snprintf(g_resp, sizeof(g_resp),
             "{\"results\":[{\"title\":\"roman <number>\","
             "\"subtitle\":\"Convert 1-3999 to numerals, or numerals back (Enter copies the form)\","
             "\"path\":\"roman \",\"payload\":\"roman \",\"score\":9000,\"action\":\"copy\"}]}");
    return g_resp;
  }

  char cooked[2048];
  int is_digits = 1;
  int is_numerals = 1;
  int saw_letter = 0;
  for (const char* p = rest; *p; ++p) {
    if (*p >= '0' && *p <= '9') {
      is_numerals = 0;
    } else if (roman_value((char)toupper((unsigned char)*p))) {
      is_digits = 0;
      saw_letter = 1;
    } else if (*p == ' ' || *p == '\t') {
      is_digits = 0;
    } else {
      is_digits = 0;
      is_numerals = 0;
      break;
    }
  }

  if (is_digits) {
    long n = strtol(rest, NULL, 10);
    if (n < ROMAN_MIN || n > ROMAN_MAX || !encode_roman((int)n, cooked, sizeof(cooked))) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
  } else if (is_numerals && saw_letter) {
    char compact[512];
    size_t cn = 0;
    for (const char* p = rest; *p; ++p) {
      if (*p == ' ' || *p == '\t') continue;
      if (cn + 1 >= sizeof(compact)) break;
      compact[cn++] = (char)toupper((unsigned char)*p);
    }
    compact[cn] = '\0';
    int n = parse_roman(compact);
    if (n == 0) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
    snprintf(cooked, sizeof(cooked), "%d", n);
  } else {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  char eq[2048], er[512];
  json_escape(cooked, eq, sizeof(eq));
  json_escape(rest, er, sizeof(er));
  snprintf(g_resp, sizeof(g_resp),
           "{\"results\":[{\"title\":\"%s\",\"subtitle\":\"roman %s (Enter copies)\","
           "\"path\":\"%s\",\"payload\":\"%s\",\"score\":9500,\"action\":\"copy\"}]}",
           eq, er, eq, eq);
  return g_resp;
}

const char* wilfred_plugin_exec(const char* json_request) {
  (void)json_request;
  strcpy(g_resp, "{\"ok\":true}");
  return g_resp;
}
