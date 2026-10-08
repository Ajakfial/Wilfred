// Wilfred plugin: morse code (`morse hello` encodes, `morse .... ..` decodes).
// Single-file native plugin (ABI v1). Build: cc -shared -fPIC -o morse.so morse.c
// (Windows: cl /nologo /LD morse.c /Femorse.dll; macOS: cc -dynamiclib -o morse.dylib morse.c)
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WILFRED_PLUGIN_ABI 1

int wilfred_plugin_abi(void) { return WILFRED_PLUGIN_ABI; }
const char* wilfred_plugin_id(void) { return "morse"; }

static char g_resp[8192];

typedef struct {
  char ch;
  const char* code;
} MorseRow;

static const MorseRow kTable[] = {
    {'A', ".-"},   {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},  {'E', "."},
    {'F', "..-."}, {'G', "--."},  {'H', "...."}, {'I', ".."},   {'J', ".---"},
    {'K', "-.-"},  {'L', ".-.."}, {'M', "--"},   {'N', "-."},   {'O', "---"},
    {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."},  {'S', "..."},  {'T', "-"},
    {'U', "..-"},  {'V', "...-"}, {'W', ".--"},  {'X', "-..-"}, {'Y', "-.--"},
    {'Z', "--.."}, {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."},
    {'9', "----."}, {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."},
};

static const char* encode_ch(char c) {
  for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i)
    if (kTable[i].ch == c) return kTable[i].code;
  return NULL;
}

static int decode_tok(const char* tok, char* out) {
  for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i)
    if (strcmp(kTable[i].code, tok) == 0) {
      *out = kTable[i].ch;
      return 1;
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

static int starts_morse(const char* q, const char** rest) {
  while (*q == ' ' || *q == '\t') ++q;
  if ((q[0] == 'm' || q[0] == 'M') && (q[1] == 'o' || q[1] == 'O') &&
      (q[2] == 'r' || q[2] == 'R') && (q[3] == 's' || q[3] == 'S') &&
      (q[4] == 'e' || q[4] == 'E') && (q[5] == ' ' || q[5] == '\t' || q[5] == '\0')) {
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
  if (!starts_morse(q, &rest)) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  while (*rest == ' ' || *rest == '\t') ++rest;
  if (*rest == '\0') {
    snprintf(g_resp, sizeof(g_resp),
             "{\"results\":[{\"title\":\"morse <text>\","
             "\"subtitle\":\"Encode text, or decode dots and dashes (Enter copies the form)\","
             "\"path\":\"morse \",\"payload\":\"morse \",\"score\":9000,\"action\":\"copy\"}]}");
    return g_resp;
  }
  // Dots/dashes/slashes only -> decode, anything else -> encode.
  int decodable = 1;
  for (const char* p = rest; *p; ++p) {
    if (*p != '.' && *p != '-' && *p != '/' && *p != ' ' && *p != '\t') {
      decodable = 0;
      break;
    }
  }
  char cooked[2048];
  size_t cn = 0;
  if (decodable) {
    char tok[16];
    size_t tn = 0;
    int ok_all = 1;
    for (const char* p = rest;; ++p) {
      if (*p == '.' || *p == '-') {
        if (tn + 1 < sizeof(tok)) tok[tn++] = *p;
      } else {
        if (tn > 0) {
          tok[tn] = '\0';
          char ch = '?';
          if (!decode_tok(tok, &ch)) ok_all = 0;
          if (cn + 1 < sizeof(cooked)) cooked[cn++] = ch;
          tn = 0;
        }
        if (*p == '/') {
          if (cn + 1 < sizeof(cooked)) cooked[cn++] = ' ';
        }
        if (*p == '\0') break;
      }
    }
    cooked[cn] = '\0';
    if (!ok_all) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
  } else {
    for (const char* p = rest; *p && cn + 8 < sizeof(cooked); ++p) {
      if (*p == ' ' || *p == '\t') {
        cooked[cn++] = '/';
        cooked[cn++] = ' ';
      } else {
        const char* c = encode_ch((char)toupper((unsigned char)*p));
        if (!c) continue;
        size_t L = strlen(c);
        memcpy(cooked + cn, c, L);
        cn += L;
        cooked[cn++] = ' ';
      }
    }
    if (cn > 0 && cooked[cn - 1] == ' ') --cn;
    cooked[cn] = '\0';
  }
  char eq[2048], er[512];
  json_escape(cooked, eq, sizeof(eq));
  json_escape(rest, er, sizeof(er));
  snprintf(g_resp, sizeof(g_resp),
           "{\"results\":[{\"title\":\"%s\",\"subtitle\":\"morse %s (Enter copies)\","
           "\"path\":\"%s\",\"payload\":\"%s\",\"score\":9500,\"action\":\"copy\"}]}",
           eq, er, eq, eq);
  return g_resp;
}

const char* wilfred_plugin_exec(const char* json_request) {
  (void)json_request;
  strcpy(g_resp, "{\"ok\":true}");
  return g_resp;
}
