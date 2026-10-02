export interface QueryExample {
  query: string;
  meaning: string;
  kind: string;
}

export const REPO_URL = 'https://github.com/Ajakfial/Wilfred';
export const RELEASES_URL = `${REPO_URL}/releases/latest`;

export const QUERY_EXAMPLES: QueryExample[] = [
  { query: 'firefox', meaning: 'Fuzzy file and app search', kind: 'search' },
  { query: '25 * 42', meaning: 'Calculator', kind: 'calc' },
  { query: '*.cpp in Projects', meaning: 'Extension plus directory filter', kind: 'search' },
  { query: 'type:image name:logo', meaning: 'Kind plus filename filter', kind: 'search' },
  { query: 'content:widget', meaning: 'Search indexed file contents', kind: 'search' },
  { query: 'timer 10m', meaning: 'Countdown timer', kind: 'mini' },
  { query: 'pomodoro', meaning: 'Pomodoro focus preset', kind: 'mini' },
  { query: 'note buy milk', meaning: 'Save a quick note', kind: 'mini' },
  { query: 'todo ship it', meaning: 'Add a todo', kind: 'mini' },
  { query: 'process chrome', meaning: 'Live process CPU and RAM', kind: 'mini' },
  { query: 'kill 1234', meaning: 'Terminate a process', kind: 'mini' },
  { query: 'hex 255', meaning: 'Number-base conversion', kind: 'dev' },
  { query: 'bit and 12 10', meaning: 'Bitwise tools', kind: 'dev' },
  { query: 'regex foo.* foobar', meaning: 'Regex tester', kind: 'dev' },
  { query: 'urlencode a b&c', meaning: 'URL codec', kind: 'dev' },
  { query: 'jwt <token>', meaning: 'Decode a JWT header and payload', kind: 'dev' },
  { query: 'uuid', meaning: 'Generate a UUID v4', kind: 'dev' },
  { query: 'json {"a":1}', meaning: 'Pretty-print JSON', kind: 'dev' },
  { query: 'media play', meaning: 'Music and media controls', kind: 'mini' },
  { query: 'ping example.com', meaning: 'Network ping summary', kind: 'mini' },
  { query: 'dns example.com', meaning: 'DNS lookup', kind: 'mini' },
  { query: 'large 10', meaning: 'Biggest files in the index', kind: 'mini' },
  { query: 'dupes', meaning: 'Duplicate file candidates', kind: 'mini' },
  { query: 'clips url', meaning: 'Clipboard history filtered by type', kind: 'mini' },
  { query: 'transcribe talk.mp3', meaning: 'Speech-to-text for audio files', kind: 'mini' },
  { query: 'ai see what is on my screen', meaning: 'Ask the AI about your screen', kind: 'ai' },
  { query: 'minimize spotify', meaning: 'Window management verbs', kind: 'mini' },
  { query: 'layout save work', meaning: 'Save a window layout', kind: 'mini' },
  { query: 'workflow review', meaning: 'Run a named multi-step workflow', kind: 'mini' },
  { query: 'ql docs hello', meaning: 'Parameterized quicklink', kind: 'mini' },
  { query: '!yt cats', meaning: 'Search macro (YouTube)', kind: 'macro' },
  { query: '100 usd to eur', meaning: 'Currency conversion', kind: 'calc' },
  { query: '#ff5500', meaning: 'Color conversion', kind: 'calc' },
  { query: 'lock', meaning: 'System command', kind: 'system' },
];

export interface Feature {
  title: string;
  body: string;
  icon: string;
}

export const FEATURES: Feature[] = [
  {
    title: 'Deep file search',
    body: 'Incremental index with WAL, filesystem watchers, fuzzy and acronym matching, and content indexing for documents and code.',
    icon: '⌕',
  },
  {
    title: 'Instant launcher',
    body: 'One summonable bar for apps, files, URLs, and web search. Global hotkey on every platform.',
    icon: '⚡',
  },
  {
    title: 'Calculator and dev utils',
    body: 'Arithmetic, units, currency, color, dates, plus hex, bits, regex, URL codec, JWT, UUID, JSON, and lorem.',
    icon: 'ƒx',
  },
  {
    title: 'Timers, notes, todos',
    body: 'Countdowns, Pomodoro presets, stopwatch, quick notes, and todos without leaving the keyboard.',
    icon: '◷',
  },
  {
    title: 'Clipboard manager',
    body: 'Persistent searchable history with pinning and type filters: urls, emails, paths, code, IPs.',
    icon: '⧉',
  },
  {
    title: 'System control',
    body: 'Live processes with killer, media keys, ping, DNS, screenshots, and session commands.',
    icon: '⏻',
  },
  {
    title: 'Workflows and quicklinks',
    body: 'Named multi-step result actions and parameterized URL templates with positional arguments.',
    icon: '⛓',
  },
  {
    title: 'Import from other launchers',
    body: 'Bring hotkeys, web searches, snippets, theme and default search from Alfred, Raycast, PowerToys Run, Flow Launcher, Ulauncher, Albert, KRunner and Rofi.',
    icon: '⇄',
  },
  {
    title: 'Private by design',
    body: 'Local-first, dependency-free core. History optional, sync opt-in to infrastructure you control.',
    icon: '◈',
  },
];

export interface InstallOption {
  os: string;
  label: string;
  commands: string[];
  note: string;
}

export const INSTALL_OPTIONS: InstallOption[] = [
  {
    os: 'Windows',
    label: 'Windows',
    commands: ['msiexec /i wilfred-<version>-windows-x64.msi', 'choco install wilfred'],
    note: 'Per-user MSI, no elevation. Publisher: Wilfred Open Contributors.',
  },
  {
    os: 'Linux',
    label: 'Linux',
    commands: ['tar -xzf wilfred-<version>-linux-x64.tar.gz', 'cd wilfred-<version>-linux-x64 && ./wilfred'],
    note: 'Needs libx11-dev for the overlay. X11 fallback when WebKitGTK is absent.',
  },
  {
    os: 'macOS',
    label: 'macOS',
    commands: ['tar -xzf wilfred-<version>-macos-arm64.tar.gz', 'cd wilfred-<version>-macos-arm64 && ./wilfred'],
    note: 'Hotkey defaults to Command+Option+W on macOS.',
  },
];

export interface ImportSource {
  os: string;
  apps: string;
}

export const IMPORT_SOURCES: ImportSource[] = [
  { os: 'macOS', apps: 'Alfred (plist bundle), Raycast (quicklinks JSON)' },
  { os: 'Windows', apps: 'PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary' },
  { os: 'Linux', apps: 'Ulauncher, Albert, KRunner / KDE, Rofi' },
];

export const IMPORT_COMMANDS: string[] = [
  'wilfred import --list',
  'wilfred import --detect',
  'wilfred import auto --dry-run',
  'wilfred import alfred --from Alfred.alfredpreferences --dry-run',
  'wilfred import flowlauncher --from Settings.json --overwrite',
];
