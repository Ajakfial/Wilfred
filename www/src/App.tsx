import { useMemo, useState } from 'react';
import { FEATURES, INSTALL_OPTIONS, QUERY_EXAMPLES, RELEASES_URL, REPO_URL } from './data';

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
            {o.label}
          </button>
        ))}
      </div>
      <div className="install-panel">
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
    { title: 'Query language', href: `${REPO_URL}/blob/main/docs/query-language.md`, body: 'Every query form, in classification order.' },
    { title: 'Configuration', href: `${REPO_URL}/blob/main/docs/configuration.md`, body: 'All wilfred.yml keys plus workflows, quicklinks, app actions.' },
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
