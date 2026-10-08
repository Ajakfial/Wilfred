// Wilfred plugin: password generator (`password`, `password 24`,
// `password words`, `password pin`). Convenience passwords with OS entropy
// (BCryptGenRandom on Windows, getentropy elsewhere); falls back to a time-seeded
// stream and says so. Single-file native plugin (ABI v1).
// Build: cc -shared -fPIC -o password.so password.c
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#if defined(__has_include)
#if __has_include(<sys/random.h>)
#include <sys/random.h>
#endif
#endif
#endif

#define WILFRED_PLUGIN_ABI 1

int wilfred_plugin_abi(void) { return WILFRED_PLUGIN_ABI; }
const char* wilfred_plugin_id(void) { return "password"; }

static char g_resp[8192];

static const char* kAlpha =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*-+=?";
static const char* kWords[] = {
    "amber",  "anchor", "apple",  "arrow",  "ash",    "atlas",  "baker",
    "bison",  "blaze",  "brass",  "bridge", "brook",  "cabin",  "cactus",
    "cedar",  "chase",  "cliff",  "cloud",  "comet",  "coral",  "crane",
    "crown",  "delta",  "drift",  "ember",  "falcon", "fern",   "flint",
    "forest", "frost",  "garnet", "glacier", "grove", "harbor", "hawk",
    "heath",  "honey",  "ivory",  "juniper", "kelp",  "lumen",  "maple",
    "marble", "meadow", "mercury", "mist",  "mosaic", "north",  "oasis",
    "onyx",   "orbit",  "otter",  "panda",  "pearl",  "pine",   "plaza",
    "prairie", "quartz", "raven", "ridge",  "river",  "robin",  "sable",
    "solar",
};
#define NWORDS (sizeof(kWords) / sizeof(kWords[0]))

static int g_weak = 0;

static unsigned long long xstate = 0;

static unsigned rng_next(unsigned bound) {
  if (bound == 0) return 0;
#if defined(_WIN32)
  // System RNG via runtime linking: bcrypt.dll ships with Windows and
  // LoadLibrary needs no import library, so this builds with MSVC and
  // MinGW alike (whose headers lack rand_s).
  {
    HMODULE h = LoadLibraryW(L"bcrypt.dll");
    if (h) {
      typedef LONG(WINAPI *GenFn)(void*, unsigned char*, unsigned long, unsigned long);
      GenFn f = (GenFn)GetProcAddress(h, "BCryptGenRandom");
      unsigned v = 0;
      /* BCRYPT_USE_SYSTEM_PREFERRED_RNG = 0x00000002; NTSTATUS >= 0 is success. */
      if (f && f(NULL, (unsigned char*)&v, (unsigned long)sizeof(v), 0x00000002) >= 0) {
        FreeLibrary(h);
        return v % bound;
      }
      FreeLibrary(h);
    }
  }
#else
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
  unsigned v = 0;
  if (getentropy(&v, sizeof(v)) == 0) return v % bound;
#endif
#endif
  g_weak = 1;
  if (!xstate) xstate = (unsigned long long)time(NULL) * 6364136223846793005ull + 1;
  xstate ^= xstate << 13;
  xstate ^= xstate >> 7;
  xstate ^= xstate << 17;
  return (unsigned)(xstate % bound);
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

static int starts_pw(const char* q, const char** rest) {
  while (*q == ' ' || *q == '\t') ++q;
  const char* w = "password";
  for (int i = 0; i < 8; ++i) {
    if (tolower((unsigned char)q[i]) != w[i]) return 0;
  }
  if (q[8] != ' ' && q[8] != '\t' && q[8] != '\0') return 0;
  *rest = q[8] ? q + 9 : q + 8;
  return 1;
}

const char* wilfred_plugin_query(const char* json_request) {
  char q[256];
  if (!req_q(json_request ? json_request : "", q, sizeof(q))) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  const char* rest = NULL;
  if (!starts_pw(q, &rest)) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  while (*rest == ' ' || *rest == '\t') ++rest;
  g_weak = 0;
  char pw[512];
  char kind[32];
  if (*rest == '\0') {
    size_t alen = strlen(kAlpha);
    for (int i = 0; i < 16; ++i) pw[i] = kAlpha[rng_next((unsigned)alen)];
    pw[16] = '\0';
    strcpy(kind, "16 chars");
  } else if ((*rest >= '0' && *rest <= '9')) {
    int len = atoi(rest);
    if (len < 4 || len > 128) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
    size_t alen = strlen(kAlpha);
    for (int i = 0; i < len && i < 500; ++i) pw[i] = kAlpha[rng_next((unsigned)alen)];
    pw[len < 500 ? len : 500] = '\0';
    snprintf(kind, sizeof(kind), "%d chars", len);
  } else {
    char w0[16];
    size_t i = 0;
    for (; rest[i] && rest[i] != ' ' && rest[i] != '\t' && i + 1 < sizeof(w0); ++i)
      w0[i] = (char)tolower((unsigned char)rest[i]);
    w0[i] = '\0';
    rest += i;
    while (*rest == ' ' || *rest == '\t') ++rest;
    if (strcmp(w0, "words") == 0) {
      int n = (*rest >= '0' && *rest <= '9') ? atoi(rest) : 4;
      if (n < 2 || n > 12) {
        strcpy(g_resp, "{\"results\":[]}");
        return g_resp;
      }
      size_t pn = 0;
      for (int w = 0; w < n && pn + 16 < sizeof(pw); ++w) {
        if (w) pw[pn++] = '-';
        const char* wd = kWords[rng_next((unsigned)NWORDS)];
        size_t L = strlen(wd);
        memcpy(pw + pn, wd, L);
        pn += L;
      }
      pw[pn] = '\0';
      snprintf(kind, sizeof(kind), "%d words", n);
    } else if (strcmp(w0, "pin") == 0) {
      int n = (*rest >= '0' && *rest <= '9') ? atoi(rest) : 6;
      if (n < 4 || n > 12) {
        strcpy(g_resp, "{\"results\":[]}");
        return g_resp;
      }
      for (int d = 0; d < n; ++d) pw[d] = (char)('0' + rng_next(10));
      pw[n] = '\0';
      snprintf(kind, sizeof(kind), "%d-digit PIN", n);
    } else {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
  }
  char sub[128];
  snprintf(sub, sizeof(sub), "%s%s (Enter copies, clipboard only — nothing is stored)",
           kind, g_weak ? ", weak RNG" : "");
  snprintf(g_resp, sizeof(g_resp),
           "{\"results\":[{\"title\":\"%s\",\"subtitle\":\"%s\","
           "\"path\":\"%s\",\"payload\":\"%s\",\"score\":9500,\"action\":\"copy\"}]}",
           pw, sub, pw, pw);
  return g_resp;
}

const char* wilfred_plugin_exec(const char* json_request) {
  (void)json_request;
  strcpy(g_resp, "{\"ok\":true}");
  return g_resp;
}
