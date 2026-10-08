import { useEffect, useMemo, useState } from 'react';
import { FEATURES, IMPORT_COMMANDS, IMPORT_SOURCES, INSTALL_OPTIONS, QUERY_EXAMPLES, RELEASES_URL, REPO_URL } from './data';

export const PLUGIN_REGISTRY_URL = 'https://ajakfial.github.io/Wilfred/plugins/registry.json';

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
  return (
    <section className="hero">
      <p className="eyebrow">Fast &middot; Deep &middot; Modular &middot; Low-resource &middot; Cross-platform</p>
      <h1>
        One search bar for
        <br />
        everything on your machine.
      </h1>
      <p className="lede">
        Wilfred is a fast, lightweight desktop search engine and application launcher for
        Windows, macOS, and Linux — files, apps, calculations, timers, notes, clipboard,
        media, and more.
      </p>
      <div className="cta-row">
        <a className="btn primary" href={RELEASES_URL}>
          Download latest
        </a>
        <a className="btn" href={REPO_URL}>
          View on GitHub
        </a>
      </div>
      <p className="hotkey">
        Summon anywhere with{' '}
        <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>W</kbd>
      </p>
    </section>
  );
}

function QueryDemo() {
  const [q, setQ] = useState('');
  const hits = useMemo(() => {
    const needle = q.trim().toLowerCase();
    if (!needle) return QUERY_EXAMPLES.slice(0, 8);
    return QUERY_EXAMPLES.filter(
      (e) =>
        e.query.toLowerCase().includes(needle) ||
        e.meaning.toLowerCase().includes(needle) ||
        e.kind.includes(needle),
    ).slice(0, 8);
  }, [q]);
  return (
    <section id="queries" className="section">
      <h2>Try the query language</h2>
      <p className="muted">Type below to filter real queries Wilfred understands.</p>
      <div className="demo-bar">
        <span className="demo-prompt" aria-hidden="true">
          ⌕
        </span>
        <input
          value={q}
          onChange={(e) => setQ(e.target.value)}
          placeholder="timer 10m"
          aria-label="Filter example queries"
        />
      </div>
      <ul className="demo-list">
        {hits.map((h) => (
          <li key={h.query} className="demo-row">
            <code>{h.query}</code>
            <span className="demo-meaning">{h.meaning}</span>
            <span className={`chip kind-${h.kind}`}>{h.kind}</span>
          </li>
        ))}
        {hits.length === 0 && <li className="demo-row empty">No examples match — try timer.</li>}
      </ul>
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
          <pre key={c}>
            <code>{c}</code>
          </pre>
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
        <pre>
          <code>wilfred config-set plugins.registry {PLUGIN_REGISTRY_URL}</code>
        </pre>
        <pre>
          <code>wilfred plugin install dice</code>
        </pre>
        <p className="muted">
          New installs need trust approval (<code>wilfred plugin approve &lt;id&gt;</code>).
          Submit yours with a <code>plugins/&lt;id&gt;/</code> pull request — see the gallery readme.
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
            ? 'The gallery index is not published yet — check back after the next plugins workflow run.'
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

function Install() {
  const [tab, setTab] = useState('Windows');
  const active = INSTALL_OPTIONS.find((o) => o.os === tab) ?? INSTALL_OPTIONS[0];
  return (
    <section id="install" className="section">
      <h2>Install</h2>
      <div className="tabs" role="tablist" aria-label="Platform">
        {INSTALL_OPTIONS.map((o) => (
          <button
            key={o.os}
            role="tab"
            aria-selected={o.os === tab}
            className={o.os === tab ? 'tab active' : 'tab'}
            onClick={() => setTab(o.os)}
          >
            {o.label}{' '}
            {o.experimental && <span className="chip experimental">Experimental</span>}
          </button>
        ))}
      </div>
      <div className="install-panel">
        {active.experimental && (
          <p className="warning" role="note">
            <strong>Experimental build (as of v23.0.5).</strong> It installs and runs, but
            should not be used as a daily driver until further testing and development
            land. Desktop (Windows, macOS, Linux) remains the stable target.
          </p>
        )}
        {active.commands.map((c) => (
          <pre key={c}>
            <code>{c}</code>
          </pre>
        ))}
        <p className="muted">{active.note}</p>
        <p>
          <a href={RELEASES_URL}>Get the latest release →</a>
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
            <h3>{l.title} →</h3>
            <p>{l.body}</p>
          </a>
        ))}
      </div>
    </section>
  );
}

export default function App() {
  return (
    <div className="page">
      <header className="nav">
        <Logo />
        <nav>
          <a href="#features">Features</a>
          <a href="#queries">Queries</a>
          <a href="#plugins">Plugins</a>
          <a href="#import">Import</a>
          <a href="#install">Install</a>
          <a href="#docs">Docs</a>
          <a className="btn small" href={REPO_URL}>
            GitHub
          </a>
        </nav>
      </header>
      <main>
        <Hero />
        <QueryDemo />
        <Features />
        <Plugins />
        <Import />
        <Install />
        <Docs />
      </main>
      <footer>
        <p>
          Wilfred is MIT-licensed. Publisher: Wilfred Open Contributors.{' '}
          <a href={REPO_URL}>Contribute on GitHub</a>.
        </p>
      </footer>
    </div>
  );
}
