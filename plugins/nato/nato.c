// Wilfred plugin: NATO phonetic alphabet (`nato hello` encodes, `nato hotel echo` decodes).
// Single-file native plugin (ABI v1). Build: cc -shared -fPIC -o nato.so nato.c
// (Windows: cl /nologo /LD nato.c /Fenato.dll; macOS: cc -dynamiclib -o nato.dylib nato.c)
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WILFRED_PLUGIN_ABI 1

int wilfred_plugin_abi(void) { return WILFRED_PLUGIN_ABI; }
const char* wilfred_plugin_id(void) { return "nato"; }

static char g_resp[8192];

typedef struct {
  char ch;
  const char* word;
} NatoRow;

static const NatoRow kTable[] = {
    {'A', "Alfa"},   {'B', "Bravo"},  {'C', "Charlie"}, {'D', "Delta"},
    {'E', "Echo"},   {'F', "Foxtrot"}, {'G', "Golf"},   {'H', "Hotel"},
    {'I', "India"},  {'J', "Juliett"}, {'K', "Kilo"},   {'L', "Lima"},
    {'M', "Mike"},   {'N', "November"}, {'O', "Oscar"}, {'P', "Papa"},
    {'Q', "Quebec"}, {'R', "Romeo"},  {'S', "Sierra"}, {'T', "Tango"},
    {'U', "Uniform"}, {'V', "Victor"}, {'W', "Whiskey"}, {'X', "X-ray"},
    {'Y', "Yankee"}, {'Z', "Zulu"},   {'0', "Zero"},   {'1', "One"},
    {'2', "Two"},    {'3', "Three"},  {'4', "Four"},   {'5', "Five"},
    {'6', "Six"},    {'7', "Seven"},  {'8', "Eight"},  {'9', "Niner"},
};

static const char* encode_ch(char c) {
  for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i)
    if (kTable[i].ch == c) return kTable[i].word;
  return NULL;
}

// Case-insensitive word -> letter/digit. Accepts common variants:
// alpha->A, xray/x-ray->X, nine->9.
static int decode_word(const char* tok, char* out) {
  char low[16];
  size_t n = 0;
  for (; tok[n] && n + 1 < sizeof(low); ++n)
    low[n] = (char)tolower((unsigned char)tok[n]);
  low[n] = '\0';
  if (strcmp(low, "alpha") == 0) {
    *out = 'A';
    return 1;
  }
  if (strcmp(low, "xray") == 0 || strcmp(low, "x-ray") == 0) {
    *out = 'X';
    return 1;
  }
  if (strcmp(low, "nine") == 0) {
    *out = '9';
    return 1;
  }
  for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i) {
    const char* w = kTable[i].word;
    size_t j = 0;
    for (; w[j] && tok[j]; ++j)
      if (tolower((unsigned char)w[j]) != tolower((unsigned char)tok[j])) break;
    if (w[j] == '\0' && tok[j] == '\0') {
      *out = kTable[i].ch;
      return 1;
    }
  }
  return 0;
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

static int starts_nato(const char* q, const char** rest) {
  while (*q == ' ' || *q == '\t') ++q;
  if ((q[0] == 'n' || q[0] == 'N') && (q[1] == 'a' || q[1] == 'A') &&
      (q[2] == 't' || q[2] == 'T') && (q[3] == 'o' || q[3] == 'O') &&
      (q[4] == ' ' || q[4] == '\t' || q[4] == '\0')) {
    *rest = q[4] ? q + 5 : q + 4;
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
  if (!starts_nato(q, &rest)) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  while (*rest == ' ' || *rest == '\t') ++rest;
  if (*rest == '\0') {
    snprintf(g_resp, sizeof(g_resp),
             "{\"results\":[{\"title\":\"nato <text>\","
             "\"subtitle\":\"Spell text as Alfa Bravo ..., or decode words back (Enter copies the form)\","
             "\"path\":\"nato \",\"payload\":\"nato \",\"score\":9000,\"action\":\"copy\"}]}");
    return g_resp;
  }

  // Decode when every whitespace-separated token is "/" or a known NATO
  // word; otherwise fall through to encode when the input is plain text.
  char copy[512];
  snprintf(copy, sizeof(copy), "%s", rest);
  int decodable = 1;
  int saw_word = 0;
  char* tok = strtok(copy, " \t");
  while (tok) {
    if (strcmp(tok, "/") != 0) {
      char ch = '?';
      if (!decode_word(tok, &ch)) {
        decodable = 0;
        break;
      }
      saw_word = 1;
    }
    tok = strtok(NULL, " \t");
  }

  char cooked[2048];
  size_t cn = 0;
  if (decodable && saw_word) {
    snprintf(copy, sizeof(copy), "%s", rest);
    int need_space = 0;
    tok = strtok(copy, " \t");
    while (tok) {
      if (strcmp(tok, "/") == 0) {
        if (cn + 1 < sizeof(cooked)) cooked[cn++] = ' ';
        need_space = 0;
      } else {
        char ch = '?';
        decode_word(tok, &ch);
        if (need_space) {
          // Words were space-separated in the NATO form; decoded letters
          // run together within a word (slashes mark word breaks).
          if (cn + 1 >= sizeof(cooked)) break;
        } else {
          need_space = 1;
        }
        if (cn + 1 < sizeof(cooked)) cooked[cn++] = ch;
      }
      tok = strtok(NULL, " \t");
    }
    cooked[cn < sizeof(cooked) ? cn : sizeof(cooked) - 1] = '\0';
  } else {
    // Encode: letters/digits -> words, space -> "/".
    int encodable = 0;
    for (const char* p = rest; *p; ++p)
      if (isalnum((unsigned char)*p) || *p == ' ' || *p == '\t') {
        encodable = 1;
        break;
      }
    if (!encodable) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
    int first = 1;
    for (const char* p = rest; *p && cn + 16 < sizeof(cooked); ++p) {
      if (*p == ' ' || *p == '\t') {
        if (cn + 3 < sizeof(cooked)) {
          cooked[cn++] = ' ';
          cooked[cn++] = '/';
          cooked[cn++] = ' ';
        }
        first = 1;
      } else if (isalnum((unsigned char)*p)) {
        const char* w = encode_ch((char)toupper((unsigned char)*p));
        if (!w) continue;
        if (!first) {
          if (cn + 1 >= sizeof(cooked)) break;
          cooked[cn++] = ' ';
        }
        size_t L = strlen(w);
        if (cn + L >= sizeof(cooked)) break;
        memcpy(cooked + cn, w, L);
        cn += L;
        first = 0;
      } else {
        continue;
      }
    }
    cooked[cn < sizeof(cooked) ? cn : sizeof(cooked) - 1] = '\0';
    if (cn == 0) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
  }
  char eq[2048], er[512];
  json_escape(cooked, eq, sizeof(eq));
  json_escape(rest, er, sizeof(er));
  snprintf(g_resp, sizeof(g_resp),
           "{\"results\":[{\"title\":\"%s\",\"subtitle\":\"nato %s (Enter copies)\","
           "\"path\":\"%s\",\"payload\":\"%s\",\"score\":9500,\"action\":\"copy\"}]}",
           eq, er, eq, eq);
  return g_resp;
}

const char* wilfred_plugin_exec(const char* json_request) {
  (void)json_request;
  strcpy(g_resp, "{\"ok\":true}");
  return g_resp;
}
