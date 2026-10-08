// Wilfred plugin: offline cheat sheets (`cheat git`, `cheat vim rebase`).
// Static tables for git / vim / tmux; filters entries by remaining tokens.
// Single-file native plugin (ABI v1). Build: cc -shared -fPIC -o cheatsheets.so cheatsheets.c
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WILFRED_PLUGIN_ABI 1

int wilfred_plugin_abi(void) { return WILFRED_PLUGIN_ABI; }
const char* wilfred_plugin_id(void) { return "cheatsheets"; }

static char g_resp[16384];

typedef struct {
  const char* cmd;
  const char* desc;
} CheatRow;

typedef struct {
  const char* topic;
  const CheatRow* rows;
  size_t n;
} CheatTopic;

static const CheatRow kGit[] = {
    {"git status -sb", "Short status with branch"},
    {"git add -p", "Stage changes hunk by hunk"},
    {"git commit -m \"msg\"", "Commit with a message"},
    {"git log --oneline -20", "Compact recent history"},
    {"git diff --staged", "Review what is about to be committed"},
    {"git checkout -b name", "Create and switch to a branch"},
    {"git switch -", "Jump back to the previous branch"},
    {"git rebase -i HEAD~3", "Rewrite the last 3 commits"},
    {"git stash push -m \"wip\"", "Shelve work in progress"},
    {"git stash pop", "Restore shelved work"},
    {"git reset --soft HEAD~1", "Undo last commit, keep changes staged"},
    {"git cherry-pick <sha>", "Apply one commit here"},
    {"git bisect start", "Binary-search a regression"},
    {"git worktree add ../hotfix", "Check out a second working tree"},
};

static const CheatRow kVim[] = {
    {":w", "Save the file"},
    {":q!", "Quit without saving"},
    {"ggVG", "Select the whole file"},
    {"ci\"", "Change inside quotes"},
    {"di(", "Delete inside parentheses"},
    {"* / #", "Search word under cursor forward/back"},
    {":%s/old/new/gc", "Find and replace with confirmation"},
    {"u / Ctrl-r", "Undo / redo"},
    {"m{a} / '{a}", "Set mark / jump to mark"},
    {"qa … q / @a", "Record and replay a macro"},
    {":split / :vsplit", "Split window horizontal/vertical"},
    {"gd", "Go to local definition"},
    {"[c / ]c", "Previous / next diff hunk"},
};

static const CheatRow kTmux[] = {
    {"tmux new -s name", "Start a named session"},
    {"tmux attach -t name", "Attach to a session"},
    {"Ctrl-b d", "Detach, keep session alive"},
    {"Ctrl-b c", "New window"},
    {"Ctrl-b ,", "Rename the window"},
    {"Ctrl-b %", "Split pane vertically"},
    {"Ctrl-b \"", "Split pane horizontally"},
    {"Ctrl-b o", "Cycle through panes"},
    {"Ctrl-b [", "Scrollback / copy mode (q quits)"},
    {"tmux ls", "List sessions"},
    {"tmux kill-session -t name", "Kill a session"},
};

static const CheatTopic kTopics[] = {
    {"git", kGit, sizeof(kGit) / sizeof(kGit[0])},
    {"vim", kVim, sizeof(kVim) / sizeof(kVim[0])},
    {"tmux", kTmux, sizeof(kTmux) / sizeof(kTmux[0])},
};

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

static int req_limit(const char* json) {
  const char* k = strstr(json, "\"limit\"");
  if (!k) return 8;
  k = strchr(k, ':');
  if (!k) return 8;
  int v = atoi(k + 1);
  return v > 0 ? v : 8;
}

static void lower_inplace(char* s) {
  for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
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

const char* wilfred_plugin_query(const char* json_request) {
  char q[512];
  if (!req_q(json_request ? json_request : "", q, sizeof(q))) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  const char* p = q;
  while (*p == ' ' || *p == '\t') ++p;
  {
    char head[8];
    size_t i = 0;
    for (; p[i] && p[i] != ' ' && p[i] != '\t' && i + 1 < sizeof(head); ++i)
      head[i] = (char)tolower((unsigned char)p[i]);
    head[i] = '\0';
    if (strcmp(head, "cheat") != 0) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
    p += i;
  }
  while (*p == ' ' || *p == '\t') ++p;
  int limit = req_limit(json_request);
  if (limit > 12) limit = 12;

  char out[16384];
  size_t on = 0;
  on += (size_t)snprintf(out + on, sizeof(out) - on, "{\"results\":[");
  int count = 0;

  if (*p == '\0') {
    // Topic list.
    for (size_t t = 0; t < sizeof(kTopics) / sizeof(kTopics[0]); ++t) {
      if (count >= limit) break;
      if (count) out[on++] = ',';
      char sub[128];
      snprintf(sub, sizeof(sub), "%u entries (Enter copies \"cheat %s\")",
               (unsigned)kTopics[t].n, kTopics[t].topic);
      char et[256], es[256];
      char title[64];
      snprintf(title, sizeof(title), "cheat %s", kTopics[t].topic);
      json_escape(title, et, sizeof(et));
      json_escape(sub, es, sizeof(es));
      on += (size_t)snprintf(out + on, sizeof(out) - on,
                             "{\"title\":\"%s\",\"subtitle\":\"%s\","
                             "\"path\":\"%s\",\"payload\":\"%s\",\"score\":9400,"
                             "\"action\":\"copy\"}",
                             et, es, et, et);
      ++count;
    }
  } else {
    // Topic + optional filter tokens.
    char topic[32];
    size_t tn = 0;
    while (*p && *p != ' ' && *p != '\t' && tn + 1 < sizeof(topic)) topic[tn++] = *p++;
    topic[tn] = '\0';
    lower_inplace(topic);
    const CheatTopic* tp = NULL;
    for (size_t t = 0; t < sizeof(kTopics) / sizeof(kTopics[0]); ++t)
      if (strcmp(kTopics[t].topic, topic) == 0) tp = &kTopics[t];
    if (!tp) {
      strcpy(g_resp, "{\"results\":[]}");
      return g_resp;
    }
    char filt[256];
    size_t fn = 0;
    while (*p && fn + 1 < sizeof(filt)) filt[fn++] = *p++;
    filt[fn] = '\0';
    lower_inplace(filt);
    for (size_t r = 0; r < tp->n && count < limit; ++r) {
      char hay[512];
      snprintf(hay, sizeof(hay), "%s %s", tp->rows[r].cmd, tp->rows[r].desc);
      lower_inplace(hay);
      if (*filt) {
        // Every space-separated token must appear.
        char ft[256];
        strcpy(ft, filt);
        int miss = 0;
        for (char* tok = strtok(ft, " \t"); tok; tok = strtok(NULL, " \t")) {
          if (!strstr(hay, tok)) {
            miss = 1;
            break;
          }
        }
        if (miss) continue;
      }
      if (count) out[on++] = ',';
      char ec[512], ed[512];
      json_escape(tp->rows[r].cmd, ec, sizeof(ec));
      json_escape(tp->rows[r].desc, ed, sizeof(ed));
      on += (size_t)snprintf(out + on, sizeof(out) - on,
                             "{\"title\":\"%s\",\"subtitle\":\"%s (%s, Enter copies)\","
                             "\"path\":\"%s\",\"payload\":\"%s\",\"score\":9500,"
                             "\"kind\":\"cheat\",\"action\":\"copy\"}",
                             ec, ed, tp->topic, ec, ec);
      ++count;
    }
  }
  if (on + 3 >= sizeof(out)) {
    strcpy(g_resp, "{\"results\":[]}");
    return g_resp;
  }
  out[on++] = ']';
  out[on++] = '}';
  out[on] = '\0';
  strcpy(g_resp, out);
  return g_resp;
}

const char* wilfred_plugin_exec(const char* json_request) {
  (void)json_request;
  strcpy(g_resp, "{\"ok\":true}");
  return g_resp;
}
