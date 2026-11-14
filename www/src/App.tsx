import { useEffect, useMemo, useState, type KeyboardEvent } from 'react';
import { FEATURES, IMPORT_COMMANDS, IMPORT_SOURCES, INSTALL_OPTIONS, QUERY_EXAMPLES, RELEASES_URL, REPO_URL } from './data';

export const PLUGIN_REGISTRY_URL = 'https://ajakfial.github.io/Wilfred/plugins/registry.json';

async function copyText(text: string) {
  try {
    await navigator.clipboard.writeText(text);
  } catch {
    const ta = document.createElement('textarea');
    ta.value = text;
    document.body.appendChild(ta);
    ta.select();
    document.execCommand('copy');
    ta.remove();
  }
}

function detectOS(): string {
  const ua = typeof navigator === 'undefined' ? '' : navigator.userAgent;
  if (/Android|iPhone|iPad/i.test(ua)) return 'desktop';
  if (/Mac/i.test(ua)) return 'macOS';
  if (/Linux|X11/i.test(ua)) return 'Linux';
  return 'Windows';
}

function CopyButton({ text, label = 'Copy' }: { text: string; label?: string }) {
  const [done, setDone] = useState(false);
  const onClick = async () => {
    await copyText(text);
    setDone(true);
    window.setTimeout(() => setDone(false), 1600);
  };
  return (
    <button type="button" className="copy" onClick={() => void onClick()} aria-live="polite">
      {done ? 'Copied' : label}
    </button>
  );
}

function CodeBlock({ code }: { code: string }) {
  return (
    <div className="code">
      <pre>
        <code>{code}</code>
      </pre>
      <CopyButton text={code} />
    </div>
  );
}

function Logo() {
  return (
    <span className="logo" aria-hidden="true">
      <svg width="28" height="28" viewBox="0 0 32 32" fill="none">
        <rect x="2" y="2" width="28" height="28" rx="8" fill="#6c8cff" />
        <circle cx="14" cy="14" r="6" stroke="#0d0e13" strokeWidth="3" />
        <line x1="19" y1="19" x2="26" y2="26" stroke="#0d0e13" strokeWidth="3" strokeLinecap="round" />
      </svg>
      <span className="logo-text">Wilfred</span>
    </span>
  );
}

function Hero() {
  const os = detectOS();
  const keys = os === 'macOS' ? ['⌘', '⌥', 'W'] : ['Ctrl', 'Alt', 'W'];
  return (
    <section className="hero">
      <div className="hero-copy">
        <p className="badge">Free and open source under the MIT license</p>
        <h1>One search bar for everything on your machine.</h1>
        <p className="lede">
          Wilfred is a fast, lightweight search tool and app launcher for Windows, macOS and
          Linux. Find files and apps, do quick math, run timers, take notes and search your
          clipboard without leaving the keyboard.
        </p>
        <div className="cta-row">
          <a className="btn primary lg" href={RELEASES_URL}>
            Download for {os}
          </a>
          <a className="btn lg" href="#install">
            Other platforms
          </a>
        </div>
        <p className="fine">No account needed. Works offline.</p>
        <p className="hotkey">
          Open it from anywhere with{' '}
          {keys.map((k, i) => (
            <span key={k}>
              {i > 0 && ' + '}
              <kbd>{k}</kbd>
            </span>
          ))}
        </p>
      </div>
      <figure className="hero-shot">
        <img
          src="screenshots/wilfred-query-help.png"
          alt="The Wilfred overlay listing mini commands for a help search"
          width={932}
          height={545}
        />
      </figure>
    </section>
  );
}

const SCREENSHOTS = [
  { src: 'screenshots/wilfred-idle.png', alt: 'The Wilfred overlay, idle with a history chip', caption: 'Summon anywhere with one hotkey' },
  { src: 'screenshots/wilfred-math-1plus3.png', alt: 'Calculator mini showing 1+3 = 4', caption: 'Instant calculator results' },
  { src: 'screenshots/wilfred-query-help.png', alt: 'help query listing mini commands', caption: 'help lists every mini command' },
];

function Screenshots() {
  return (
    <section id="screens" className="section">
      <h2>Screens</h2>
      <p className="muted">Real captures of the desktop overlay.</p>
      <div className="shot-grid">
        {SCREENSHOTS.map((s) => (
          <figure key={s.src} className="shot">
            <img src={s.src} alt={s.alt} width={932} height={545} loading="lazy" />
            <figcaption>{s.caption}</figcaption>
          </figure>
        ))}
      </div>
    </section>
  );
}

const KINDS = ['all', ...Array.from(new Set(QUERY_EXAMPLES.map((e) => e.kind)))];

function QueryDemo() {
  const [q, setQ] = useState('');
  const [kind, setKind] = useState('all');
  const [sel, setSel] = useState(0);
  const [copied, setCopied] = useState<string | null>(null);
  const hits = useMemo(() => {
    const needle = q.trim().toLowerCase();
    return QUERY_EXAMPLES.filter(
      (e) =>
        (kind === 'all' || e.kind === kind) &&
        (!needle ||
          e.query.toLowerCase().includes(needle) ||
          e.meaning.toLowerCase().includes(needle) ||
          e.kind.includes(needle)),
    ).slice(0, 8);
  }, [q, kind]);
  const pick = async (query: string) => {
    await copyText(query);
    setCopied(query);
    window.setTimeout(() => setCopied((c) => (c === query ? null : c)), 1400);
  };
  const onKey = (e: KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      setSel((v) => Math.min(v + 1, hits.length - 1));
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      setSel((v) => Math.max(v - 1, 0));
    } else if (e.key === 'Enter' && hits[sel]) {
      void pick(hits[sel].query);
    }
  };
  return (
    <section id="queries" className="section">
      <h2>Try the query language</h2>
      <p className="muted">
        Type to filter real queries Wilfred understands. Use the arrow keys and Enter, or click a
        row, to copy it.
      </p>
      <div className="overlay">
        <div className="demo-bar">
          <span className="demo-prompt" aria-hidden="true">
            ⌕
          </span>
          <input
            value={q}
            onChange={(e) => {
              setQ(e.target.value);
              setSel(0);
            }}
            onKeyDown={onKey}
            placeholder="Try: timer 10m"
            aria-label="Filter example queries"
          />
          {q && (
            <button
              type="button"
              className="clear"
              onClick={() => {
                setQ('');
                setSel(0);
              }}
            >
              Clear
            </button>
          )}
        </div>
        <div className="filters" role="group" aria-label="Filter by type">
          {KINDS.map((k) => (
            <button
              key={k}
              type="button"
              className="filter"
              aria-pressed={kind === k}
              onClick={() => {
                setKind(k);
                setSel(0);
              }}
            >
              {k}
            </button>
          ))}
        </div>
        <ul className="demo-list">
          {hits.map((h, i) => (
            <li key={h.query}>
              <button
                type="button"
                className={i === sel ? 'demo-row selected' : 'demo-row'}
                onClick={() => void pick(h.query)}
                onMouseEnter={() => setSel(i)}
              >
                <code>{h.query}</code>
                <span className="demo-meaning">{h.meaning}</span>
                <span className="row-end">
                  <span className={`chip kind-${h.kind}`}>{h.kind}</span>
                  <span className="hint">{copied === h.query ? 'Copied' : 'Copy'}</span>
                </span>
              </button>
            </li>
          ))}
          {hits.length === 0 && (
            <li className="demo-row empty">No examples match. Try “timer” or clear the filters.</li>
          )}
        </ul>
        <p className="overlay-foot" aria-live="polite">
          Showing {hits.length} of {QUERY_EXAMPLES.length} examples
        </p>
      </div>
    </section>
  );
}

function Features() {
  return (
    <section id="features" className="section">
      <h2>Features</h2>
      <div className="grid">
        {FEATURES.map((f) => (
          <article key={f.title} className="card">
            <div className="card-icon" aria-hidden="true">
              {f.icon}
            </div>
            <h3>{f.title}</h3>
            <p>{f.body}</p>
          </article>
        ))}
      </div>
    </section>
  );
}

function Import() {
  return (
    <section id="import" className="section">
      <h2>Switching launchers? Bring your settings</h2>
      <p className="muted">
        Wilfred imports hotkeys, custom web searches (as macros + quicklinks),
        snippets, aliases, theme and the default search template. Merging is the
        default; <code>--overwrite</code> replaces conflicts, <code>--dry-run</code> previews
        without writing, and a <code>.pre-import.bak</code> backup is written before every real import.
      </p>
      <ul className="demo-list">
        {IMPORT_SOURCES.map((s) => (
          <li key={s.os} className="demo-row">
            <code>{s.os}</code>
            <span className="demo-meaning">{s.apps}</span>
          </li>
        ))}
      </ul>
      <div className="install-panel">
        {IMPORT_COMMANDS.map((c) => (
          <CodeBlock key={c} code={c} />
        ))}
        <p className="muted">See docs/import.md for per-launcher mapping and merge semantics.</p>
      </div>
    </section>
  );
}

interface GalleryArtifact {
  url: string;
  sha256: string;
}

interface GalleryEntry {
  id: string;
  version?: string;
  kind?: string;
  description?: string;
  permissions?: string[];
  artifacts?: Record<string, GalleryArtifact>;
}

function Plugins() {
  const [q, setQ] = useState('');
  const [entries, setEntries] = useState<GalleryEntry[] | null>(null);
  const [copied, setCopied] = useState<string | null>(null);
  useEffect(() => {
    let live = true;
    fetch(`${import.meta.env.BASE_URL}plugins/registry.json`)
      .then((r) => (r.ok ? r.json() : Promise.reject(new Error(`HTTP ${r.status}`))))
      .then((d) => {
        if (live) setEntries(Array.isArray(d?.plugins) ? d.plugins : []);
      })
      .catch(() => {
        if (live) setEntries([]);
      });
    return () => {
      live = false;
    };
  }, []);
  const hits = useMemo(() => {
    const needle = q.trim().toLowerCase();
    const list = entries ?? [];
    if (!needle) return list;
    return list.filter((e) =>
      [e.id, e.description ?? '', e.kind ?? ''].some((s) => s.toLowerCase().includes(needle)),
    );
  }, [q, entries]);
  const copyInstall = async (id: string) => {
    const cmd = `wilfred plugin install ${id}`;
    try {
      await navigator.clipboard.writeText(cmd);
    } catch {
      const ta = document.createElement('textarea');
      ta.value = cmd;
      document.body.appendChild(ta);
      ta.select();
      document.execCommand('copy');
      ta.remove();
    }
    setCopied(id);
    window.setTimeout(() => setCopied((c) => (c === id ? null : c)), 1600);
  };
  return (
    <section id="plugins" className="section">
      <h2>Plugins</h2>
      <p className="muted">
        Flagship and community plugins, built per platform by CI and published
        with sha256 fingerprints. Point Wilfred at the gallery once, then
        install by id:
      </p>
      <div className="install-panel">
        <CodeBlock code={`wilfred config-set plugins.registry ${PLUGIN_REGISTRY_URL}`} />
        <CodeBlock code="wilfred plugin install dice" />
        <p className="muted">
          New installs need trust approval (<code>wilfred plugin approve &lt;id&gt;</code>).
          Submit yours with a <code>plugins/&lt;id&gt;/</code> pull request – see the gallery readme.
        </p>
      </div>
      <div className="demo-bar">
        <span className="demo-prompt" aria-hidden="true">
          ⌕
        </span>
        <input
          value={q}
          onChange={(e) => setQ(e.target.value)}
          placeholder="cheat"
          aria-label="Filter plugins"
        />
      </div>
      {entries === null ? (
        <p className="muted">Loading the gallery…</p>
      ) : hits.length === 0 ? (
        <p className="muted">
          {entries.length === 0
            ? 'The gallery index is not published yet – check back after the next plugins workflow run.'
            : 'No plugins match.'}
        </p>
      ) : (
        <div className="grid">
          {hits.map((e) => {
            const plats = ['windows', 'macos', 'linux'].filter((p) => e.artifacts?.[p]?.url);
            return (
              <article key={e.id} className="card">
                <h3>
                  {e.id}
                  {e.version ? <span className="muted"> v{e.version}</span> : null}
                </h3>
                <p>{e.description || 'No description.'}</p>
                <p className="muted">
                  {e.kind ?? 'native'}
                  {plats.length > 0 ? ` · ${plats.join(' / ')}` : ' · source only'}
                  {(e.permissions?.length ?? 0) > 0 ? ` · perms: ${e.permissions!.join(', ')}` : ''}
                </p>
                <p>
                  <button
                    type="button"
                    className="btn small"
                    onClick={() => void copyInstall(e.id)}
                  >
                    {copied === e.id ? 'Copied!' : 'Copy install command'}
                  </button>
                </p>
              </article>
            );
          })}
        </div>
      )}
    </section>
  );
}

const COMPARE_COLS = [
  ['wilfred', 'Wilfred'],
  ['alfred', 'Alfred'],
  ['raycast', 'Raycast'],
  ['flow', 'Flow Launcher'],
  ['ulauncher', 'Ulauncher'],
] as const;

function Comparison() {
  const rows = [
    { feature: 'Cross-platform (Win/macOS/Linux/BSD/Android/iOS)', wilfred: 'Yes', alfred: 'macOS only', raycast: 'macOS/Win', flow: 'Windows only', ulauncher: 'Linux only' },
    { feature: 'Open source', wilfred: 'Yes (MIT)', alfred: 'No', raycast: 'No', flow: 'Yes', ulauncher: 'Yes' },
    { feature: 'No third-party dependencies', wilfred: 'Yes', alfred: 'N/A', raycast: 'N/A', flow: 'No', ulauncher: 'No' },
    { feature: 'Files + apps + calc in one bar', wilfred: 'Yes', alfred: 'Yes', raycast: 'Yes', flow: 'Yes', ulauncher: 'Yes' },
    { feature: 'Built-in timers, notes, clipboard', wilfred: 'Yes', alfred: 'Workflows', raycast: 'Extensions', flow: 'Plugins', ulauncher: 'Extensions' },
    { feature: 'Import settings from others', wilfred: 'Yes', alfred: 'No', raycast: 'No', flow: 'No', ulauncher: 'No' },
    { feature: 'Plugin gallery (sha256)', wilfred: 'Yes', alfred: 'Workflows', raycast: 'Store', flow: 'Plugins', ulauncher: 'Extensions' },
    { feature: 'Local HTTP API', wilfred: 'Yes', alfred: 'No', raycast: 'No', flow: 'No', ulauncher: 'No' },
    { feature: 'Offline-first / no account', wilfred: 'Yes', alfred: 'Yes', raycast: 'Account', flow: 'Yes', ulauncher: 'Yes' },
  ];
  return (
    <section id="compare" className="section">
      <h2>How Wilfred compares</h2>
      <p className="muted">
        A quick, honest comparison against the launchers Wilfred can import settings from.
      </p>
      <div className="table-wrap" role="region" aria-label="Launcher comparison table" tabIndex={0}>
        <table>
          <thead>
            <tr>
              <th scope="col">Feature</th>
              {COMPARE_COLS.map(([k, name]) => (
                <th key={k} scope="col" className={k === 'wilfred' ? 'col-us' : undefined}>
                  {name}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((r) => (
              <tr key={r.feature}>
                <th scope="row">{r.feature}</th>
                {COMPARE_COLS.map(([k]) => {
                  const v = r[k];
                  const cls = [k === 'wilfred' ? 'col-us' : '', v === 'No' ? 'no' : ''].join(' ').trim();
                  return (
                    <td key={k} className={cls || undefined}>
                      {v.startsWith('Yes') && (
                        <span className="tick" aria-hidden="true">
                          ✓{' '}
                        </span>
                      )}
                      {v}
                    </td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}

function Install() {
  const [tab, setTab] = useState(() => {
    const d = detectOS();
    return INSTALL_OPTIONS.some((o) => o.os === d) ? d : 'Windows';
  });
  const active = INSTALL_OPTIONS.find((o) => o.os === tab) ?? INSTALL_OPTIONS[0];
  const onKey = (e: KeyboardEvent<HTMLButtonElement>, i: number) => {
    if (e.key !== 'ArrowRight' && e.key !== 'ArrowLeft') return;
    e.preventDefault();
    const n = INSTALL_OPTIONS.length;
    const next = INSTALL_OPTIONS[(i + (e.key === 'ArrowRight' ? 1 : n - 1)) % n];
    setTab(next.os);
    document.getElementById(`tab-${next.os}`)?.focus();
  };
  return (
    <section id="install" className="section">
      <h2>Install</h2>
      <p className="muted">Choose your platform, then copy the commands.</p>
      <div className="tabs" role="tablist" aria-label="Platform">
        {INSTALL_OPTIONS.map((o, i) => (
          <button
            key={o.os}
            id={`tab-${o.os}`}
            type="button"
            role="tab"
            aria-selected={o.os === tab}
            aria-controls="install-panel"
            tabIndex={o.os === tab ? 0 : -1}
            className={o.os === tab ? 'tab active' : 'tab'}
            onClick={() => setTab(o.os)}
            onKeyDown={(e) => onKey(e, i)}
          >
            {o.label}
            {o.experimental && <span className="chip experimental">Experimental</span>}
          </button>
        ))}
      </div>
      <div
        className="install-panel"
        id="install-panel"
        role="tabpanel"
        aria-labelledby={`tab-${active.os}`}
      >
        {active.experimental && (
          <p className="warning" role="note">
            <strong>Experimental build (as of v46.5.8).</strong> It installs and runs, but
            should not be used as a daily driver until further testing and development
            land. Desktop (Windows, macOS, Linux) remains the stable target.
          </p>
        )}
        {active.commands.map((c) => (
          <CodeBlock key={c} code={c} />
        ))}
        <p className="muted">{active.note}</p>
        <p>
          <a className="btn primary" href={RELEASES_URL}>
            Get the latest release
          </a>
        </p>
      </div>
    </section>
  );
}

function Docs() {
  const links = [
    { title: 'Import', href: `${REPO_URL}/blob/main/docs/import.md`, body: 'Bring settings from Alfred, Raycast, PowerToys, Flow Launcher, Ulauncher and more.' },
    { title: 'Plugins', href: `${REPO_URL}/blob/main/docs/plugins.md`, body: 'Write native or stdio plugins, publish them to the gallery via pull request.' },
    { title: 'Localization', href: `${REPO_URL}/blob/main/docs/localization.md`, body: 'UI languages, catalog files, and adding a translation.' },
    { title: 'Query language', href: `${REPO_URL}/blob/main/docs/query-language.md`, body: 'Every query form, in classification order.' },
    { title: 'Configuration', href: `${REPO_URL}/blob/main/docs/configuration.md`, body: 'All wilfred.yml keys plus pins, themes, workflows, quicklinks, app actions.' },
    { title: 'Sync and backup', href: `${REPO_URL}/blob/main/docs/sync-and-backup.md`, body: 'Portable archives, remote sync, and optional password encryption.' },
    { title: 'Installer', href: `${REPO_URL}/blob/main/docs/installer.md`, body: 'MSI and Chocolatey packaging details.' },
    { title: 'Signing', href: `${REPO_URL}/blob/main/docs/signing.md`, body: 'Publisher identity and Authenticode trust.' },
    { title: 'Building', href: `${REPO_URL}/blob/main/docs/building.md`, body: 'CMake, scripts, and packaging pipeline.' },
    { title: 'Architecture', href: `${REPO_URL}/blob/main/docs/architecture.md`, body: 'How the engine, index, and overlay fit together.' },
  ];
  return (
    <section id="docs" className="section">
      <h2>Docs</h2>
      <div className="grid">
        {links.map((l) => (
          <a key={l.title} className="card link" href={l.href}>
            <h3>{l.title}</h3>
            <p>{l.body}</p>
          </a>
        ))}
      </div>
    </section>
  );
}

const NAV = [
  ['screens', 'Screens'],
  ['queries', 'Try it'],
  ['features', 'Features'],
  ['compare', 'Compare'],
  ['plugins', 'Plugins'],
  ['import', 'Import'],
  ['install', 'Install'],
  ['docs', 'Docs'],
] as const;
const NAV_IDS: readonly string[] = NAV.map(([id]) => id);

function useActiveSection(ids: readonly string[]) {
  const [active, setActive] = useState('');
  useEffect(() => {
    const els = ids
      .map((id) => document.getElementById(id))
      .filter((el): el is HTMLElement => el !== null);
    const io = new IntersectionObserver(
      (entries) => {
        const top = entries
          .filter((e) => e.isIntersecting)
          .sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top)[0];
        if (top) setActive(top.target.id);
      },
      { rootMargin: '-80px 0px -60% 0px' },
    );
    els.forEach((el) => io.observe(el));
    return () => io.disconnect();
  }, [ids]);
  return active;
}

export default function App() {
  const active = useActiveSection(NAV_IDS);
  const [open, setOpen] = useState(false);
  return (
    <div className="page" id="top">
      <a className="skip" href="#main">
        Skip to content
      </a>
      <header className="nav">
        <div className="nav-in">
          <a className="brand" href="#top" aria-label="Wilfred home">
            <Logo />
          </a>
          <button
            type="button"
            className="btn small menu-btn"
            aria-expanded={open}
            aria-controls="site-nav"
            onClick={() => setOpen((o) => !o)}
          >
            {open ? 'Close' : 'Menu'}
          </button>
          <nav id="site-nav" className={open ? 'open' : undefined} aria-label="Sections">
            {NAV.map(([id, label]) => (
              <a
                key={id}
                href={`#${id}`}
                aria-current={active === id ? 'true' : undefined}
                onClick={() => setOpen(false)}
              >
                {label}
              </a>
            ))}
            <a className="btn small" href={REPO_URL}>
              GitHub
            </a>
          </nav>
        </div>
      </header>
      <main id="main">
        <Hero />
        <Screenshots />
        <QueryDemo />
        <Features />
        <Comparison />
        <Plugins />
        <Import />
        <Install />
        <Docs />
      </main>
      <footer>
        <div className="foot-in">
          <p>Wilfred is MIT-licensed. Publisher: Wilfred Open Contributors.</p>
          <nav aria-label="Footer">
            <a href={REPO_URL}>GitHub</a>
            <a href={RELEASES_URL}>Releases</a>
            <a href={`${REPO_URL}/blob/main/CONTRIBUTING.md`}>Contribute</a>
            <a href="#top">Back to top</a>
          </nav>
        </div>
      </footer>
    </div>
  );
}
