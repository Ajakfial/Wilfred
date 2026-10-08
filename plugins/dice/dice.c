// Wilfred plugin: dice roller (`3d6`, `d20`, `4d6+2`).
// Single-file native plugin (ABI v1). Build: cc -shared -fPIC -o dice.so dice.c
// (Windows: cl /nologo /LD dice.c /Fedice.dll; macOS: cc -dynamiclib -o dice.dylib dice.c)
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WILFRED_PLUGIN_ABI 1

int wilfred_plugin_abi(void) { return WILFRED_PLUGIN_ABI; }
const char* wilfred_plugin_id(void) { return "dice"; }

static char g_resp[8192];

static unsigned long long fnv(const char* p, size_t n) {
  unsigned long long h = 1469598103934665603ull;
  for (size_t i = 0; i < n; ++i) {
    h ^= (unsigned char)p[i];
    h *= 1099511628211ull;
  }
  return h;
}

// Extract the "q" string from {"op":"query","q":"...","limit":N}.
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

// Parse [roll ]NdM[+-K]. Returns 1 on match.
static int parse_roll(const char* q, int* n, int* m, int* k) {
  while (*q == ' ' || *q == '\t') ++q;
  if ((q[0] == 'r' || q[0] == 'R') && (q[1] == 'o' || q[1] == 'O') &&
      (q[2] == 'l' || q[2] == 'L') && (q[3] == 'l' || q[3] == 'L') && q[4] == ' ') {
    q += 5;
    while (*q == ' ') ++q;
  }
  *n = 0;
  while (*q >= '0' && *q <= '9') {
    *n = *n * 10 + (*q - '0');
    ++q;
  }
  if (*n == 0) *n = 1;
  if (*q != 'd' && *q != 'D') return 0;
  ++q;
  *m = 0;
  while (*q >= '0' && *q <= '9') {
    *m = *m * 10 + (*q - '0');
    ++q;
  }
  *k = 0;
  if (*q == '+' || *q == '-') {
    int sign = (*q == '-') ? -1 : 1;
    ++q;
    int v = 0;
    int any = 0;
    while (*q >= '0' && *q <= '9') {
      v = v * 10 + (*q - '0');
      ++q;
      any = 1;
    }
    if (!any) return 0;
    *k = sign * v;
  }
  while (*q == ' ' || *q == '\t') ++q;
  if (*q != '\0') return 0;
  if (*n < 1 || *n > 100 || *m < 2 || *m > 1000) return 0;
  return 1;
}

const char* wilfred_plugin_query(const char* json_request) {
  char q[256];
  if (!req_q(json_request ? json_request : "", q, sizeof(q))) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  int n = 0, m = 0, k = 0;
  if (!parse_roll(q, &n, &m, &k)) {
    // Usage hint only on the bare trigger words.
    char low[16];
    size_t i = 0;
    for (; q[i] && i + 1 < sizeof(low); ++i) low[i] = (char)tolower((unsigned char)q[i]);
    low[i] = '\0';
    if (strcmp(low, "dice") != 0 && strcmp(low, "roll") != 0 && strcmp(low, "d") != 0) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
    snprintf(g_resp, sizeof(g_resp),
             "{\"results\":[{\"title\":\"dice NdM[+K]\","
             "\"subtitle\":\"Roll dice, e.g. 3d6 or d20+2 (Enter copies the form)\","
             "\"path\":\"3d6\",\"payload\":\"3d6\",\"score\":9000,\"action\":\"copy\"}]}");
    return g_resp;
  }
  srand((unsigned)(time(NULL) ^ fnv(q, strlen(q))));
  char rolls[1024];
  size_t rn = 0;
  long total = k;
  for (int i = 0; i < n; ++i) {
    int r = 1 + rand() % m;
    total += r;
    if (rn + 32 >= sizeof(rolls)) break;  // keep room for the next number
    if (i) {
      rolls[rn++] = ' ';
      rolls[rn++] = '+';
      rolls[rn++] = ' ';
    }
    rn += (size_t)snprintf(rolls + rn, sizeof(rolls) - rn, "%d", r);
  }
  rolls[rn < sizeof(rolls) ? rn : sizeof(rolls) - 1] = '\0';
  char spec[64];
  if (k > 0)
    snprintf(spec, sizeof(spec), "%dd%d+%d", n, m, k);
  else if (k < 0)
    snprintf(spec, sizeof(spec), "%dd%d%d", n, m, k);
  else
    snprintf(spec, sizeof(spec), "%dd%d", n, m);
  char total_s[32];
  snprintf(total_s, sizeof(total_s), "%ld", total);
  char k_s[16] = {0};
  if (k > 0)
    snprintf(k_s, sizeof(k_s), " %+d", k);
  else if (k < 0)
    snprintf(k_s, sizeof(k_s), " %d", k);
  snprintf(g_resp, sizeof(g_resp),
           "{\"results\":[{\"title\":\"%s \\u2192 %s\",\"subtitle\":\"%s%s\","
           "\"path\":\"%s\",\"payload\":\"%s\",\"score\":9500,\"action\":\"copy\"}]}",
           spec, total_s, rolls, k_s, spec, total_s);
  return g_resp;
}

const char* wilfred_plugin_exec(const char* json_request) {
  (void)json_request;
  strcpy(g_resp, "{\"ok\":true}");
  return g_resp;
}
