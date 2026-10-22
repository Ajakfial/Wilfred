import { onHostMessage, sendToHost, type HostEvent } from "./bridge";

declare const __LICENSE_TEXT__: string;
declare const __SETUP_VERSION__: string;

type Step = "welcome" | "license" | "options" | "progress" | "finish" | "error";

const STEPS: ReadonlyArray<{ id: Step; label: string }> = [
  { id: "welcome", label: "Welcome" },
  { id: "license", label: "License" },
  { id: "options", label: "Options" },
  { id: "progress", label: "Install" },
  { id: "finish", label: "Finish" },
];

const state = {
  step: "welcome" as Step,
  version: __SETUP_VERSION__,
  installDir: "%LOCALAPPDATA%\\Wilfred",
  accepted: false,
  launch: true,
  percent: 0,
  phase: "Starting",
  action: "",
  error: "",
};

const els = {
  view: document.getElementById("view") as HTMLElement,
  steps: document.getElementById("steps") as HTMLElement,
  version: document.getElementById("version") as HTMLElement,
  status: document.getElementById("status") as HTMLElement,
  back: document.getElementById("back") as HTMLButtonElement,
  next: document.getElementById("next") as HTMLButtonElement,
};

/* Preview-only deep link (e.g. dist/index.html?step=license). The real host
   navigates to about:blank, so this never affects the wizard there. */
const requestedStep = new URLSearchParams(window.location.search).get("step") as Step | null;
if (
  requestedStep &&
  (requestedStep === "error" || STEPS.some((entry) => entry.id === requestedStep))
) {
  state.step = requestedStep;
  if (requestedStep === "license" || requestedStep === "options" || requestedStep === "error") {
    state.accepted = true;
  }
  if (requestedStep === "progress") {
    state.percent = 46;
    state.phase = "Copying program files";
    state.action = "Copying new files: ui\\overlay\\app.bundle.js";
  }
  if (requestedStep === "finish") state.percent = 100;
  if (requestedStep === "error") state.error = "Error 1603. Fatal error during installation.";
}

const CHECK_ICON =
  '<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4.5 12.5l5 5 10-11"/></svg>';
const ERROR_ICON =
  '<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" aria-hidden="true"><path d="M12 7v6"/><path d="M12 17h.01"/></svg>';

function escapeHtml(value: string): string {
  return value
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function stepIndex(): number {
  if (state.step === "error") return 3;
  const index = STEPS.findIndex((entry) => entry.id === state.step);
  return index < 0 ? 0 : index;
}

/* ------------------------------------------------------------------ */
/* Rendering                                                          */
/* ------------------------------------------------------------------ */

function render(): void {
  const current = stepIndex();
  els.steps.innerHTML = STEPS.map((entry, index) => {
    const cls = index < current ? "done" : index === current ? "active" : "";
    const marker = index < current ? CHECK_ICON_SMALL : String(index + 1);
    return `<li class="${cls}"><span class="dot">${marker}</span>${escapeHtml(entry.label)}</li>`;
  }).join("");
  els.version.textContent = state.version;
  renderView();
  renderFooter();
}

const CHECK_ICON_SMALL =
  '<svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3.4" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 12.5l5.5 5.5L20 6.5"/></svg>';

function renderView(): void {
  switch (state.step) {
    case "welcome":
      els.view.innerHTML = viewWelcome();
      break;
    case "license":
      els.view.innerHTML = viewLicense();
      renderLicenseText();
      bindLicense();
      break;
    case "options":
      els.view.innerHTML = viewOptions();
      bindOptions();
      break;
    case "progress":
      els.view.innerHTML = viewProgress();
      updateProgress();
      break;
    case "finish":
      els.view.innerHTML = viewFinish();
      break;
    case "error":
      els.view.innerHTML = viewError();
      break;
  }
}

function viewWelcome(): string {
  return `
    <p class="eyebrow">Windows &middot; x64</p>
    <h1>Welcome to Wilfred</h1>
    <p class="lede">
      A fast, local-first launcher for everything on your machine &mdash; apps, files,
      settings, clipboard history, notes and AI, one keystroke away.
    </p>
    <div class="grid">
      <div class="card">
        <h3>Find anything</h3>
        <p>Search apps, files, folders, settings, windows and browser history as you type.</p>
      </div>
      <div class="card">
        <h3>Ask Wilfred</h3>
        <p>Multiturn AI chat with the assistant you configure, right from the launcher.</p>
      </div>
      <div class="card">
        <h3>Stay on track</h3>
        <p>Reminders, quick notes and clipboard history that survive a restart.</p>
      </div>
      <div class="card">
        <h3>Private by default</h3>
        <p>Your index and history stay on this PC. No telemetry, no account required.</p>
      </div>
    </div>
    <div class="note">
      <span class="icon">&#9432;</span>
      <span>
        Wilfred installs for your Windows account only, in
        <code>${escapeHtml(state.installDir)}</code>, so no administrator rights are needed.
      </span>
    </div>`;
}

function viewLicense(): string {
  return `
    <p class="eyebrow">Step 2</p>
    <h1 class="tight">License agreement</h1>
    <p class="lede">
      Wilfred is open-source software. Please review the terms below before installing.
    </p>
    <div class="license" id="license" tabindex="0" role="region" aria-label="License agreement"></div>
    <label class="check">
      <input type="checkbox" id="accept" ${state.accepted ? "checked" : ""} />
      I accept the terms in the License Agreement
    </label>`;
}

function renderLicenseText(): void {
  const host = document.getElementById("license");
  if (!host) return;
  let paragraph: string[] = [];

  const flush = (): void => {
    if (paragraph.length === 0) return;
    const p = document.createElement("p");
    p.textContent = paragraph.join(" ");
    host.append(p);
    paragraph = [];
  };

  for (const line of __LICENSE_TEXT__.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (trimmed.startsWith("# ")) {
      flush();
      const heading = document.createElement("h2");
      heading.textContent = trimmed.slice(2).trim();
      host.append(heading);
    } else if (trimmed === "") {
      flush();
    } else {
      paragraph.push(trimmed);
    }
  }
  flush();
}

function viewOptions(): string {
  return `
    <p class="eyebrow">Step 3</p>
    <h1 class="tight">Ready when you are</h1>
    <p class="lede">Setup will install Wilfred for your account and add it to your Start Menu.</p>
    <div class="option-row">
      <div>
        <h3>Install location</h3>
        <p>Program files, the overlay UI and the default configuration.</p>
        <span class="path">${escapeHtml(state.installDir)}</span>
      </div>
    </div>
    <div class="option-row">
      <div>
        <h3>Launch Wilfred after setup</h3>
        <p>Start the launcher as soon as installation finishes.</p>
      </div>
      <label class="switch">
        <input type="checkbox" id="launch" ${state.launch ? "checked" : ""} aria-label="Launch Wilfred after setup" />
        <span class="track"></span>
      </label>
    </div>
    <div class="note" style="margin-top: 14px">
      <span class="icon">&#9432;</span>
      <span>
        Setup also creates desktop and Start Menu shortcuts and adds the install folder to your
        user <code>PATH</code> so the <code>wilfred</code> command works in terminals.
        Uninstall any time from Settings &rarr; Apps.
      </span>
    </div>`;
}

function viewProgress(): string {
  return `
    <p class="eyebrow">Installing</p>
    <h1 class="tight">Setting things up</h1>
    <p class="lede">This usually takes less than a minute. Windows Installer is doing the work.</p>
    <div class="progress-wrap">
      <div class="bar" id="bar"><div class="bar-fill" id="bar-fill"></div></div>
      <div class="progress-meta">
        <span><span class="spinner"></span><span id="phase">${escapeHtml(state.phase)}</span></span>
        <span class="pct" id="pct">0%</span>
      </div>
      <div class="action" id="action"></div>
    </div>`;
}

function viewFinish(): string {
  const launch = state.launch ? "Wilfred is starting &mdash; look for it in the taskbar." : "You can open Wilfred from the Start Menu.";
  return `
    <div class="result">
      <div class="badge ok">${CHECK_ICON}</div>
      <p class="eyebrow">Complete</p>
      <h1>Wilfred is ready</h1>
      <p class="lede">${launch}</p>
      <div class="detail">
        <strong>Installed to</strong><br />
        ${escapeHtml(state.installDir)}<br /><br />
        <strong>Version</strong> ${escapeHtml(state.version)}
      </div>
    </div>`;
}

function viewError(): string {
  return `
    <div class="result">
      <div class="badge error">${ERROR_ICON}</div>
      <p class="eyebrow">Setup failed</p>
      <h1>Couldn't finish installing</h1>
      <p class="lede">Windows Installer reported an error and rolled the installation back.</p>
      <p class="error-text">${escapeHtml(state.error || "The installer exited unexpectedly.")}</p>
      <div class="detail">
        You can try again, or run the <strong>.msi</strong> package directly if the problem
        keeps happening. Details are written to <strong>%TEMP%\\wilfred-setup.log</strong>.
      </div>
    </div>`;
}

function bindLicense(): void {
  const accept = document.getElementById("accept") as HTMLInputElement | null;
  accept?.addEventListener("change", () => {
    state.accepted = accept.checked;
    els.next.disabled = !state.accepted;
    els.status.textContent = state.accepted ? "" : "Accept the license to continue.";
  });
}

function bindOptions(): void {
  const launch = document.getElementById("launch") as HTMLInputElement | null;
  launch?.addEventListener("change", () => {
    state.launch = launch.checked;
  });
}

function renderFooter(): void {
  els.status.textContent = "";
  els.back.hidden = state.step === "welcome" || state.step === "progress" || state.step === "finish";
  els.next.hidden = state.step === "progress";
  els.back.disabled = false;
  els.next.disabled = false;

  switch (state.step) {
    case "welcome":
      els.next.textContent = "Continue";
      break;
    case "license":
      els.next.textContent = "Continue";
      els.next.disabled = !state.accepted;
      els.status.textContent = state.accepted ? "" : "Accept the license to continue.";
      break;
    case "options":
      els.next.textContent = "Install";
      break;
    case "progress":
      els.status.textContent = "Setup is installing Wilfred. Keep this window open.";
      break;
    case "finish":
      els.next.textContent = "Finish";
      break;
    case "error":
      els.back.hidden = false;
      els.back.textContent = "Close";
      els.next.textContent = "Try again";
      break;
  }
}

/* ------------------------------------------------------------------ */
/* Progress updates (no full re-render, so the bar animates smoothly) */
/* ------------------------------------------------------------------ */

function updateProgress(): void {
  const bar = document.getElementById("bar");
  const fill = document.getElementById("bar-fill");
  const pct = document.getElementById("pct");
  const phase = document.getElementById("phase");
  const action = document.getElementById("action");
  if (!bar || !fill || !pct || !phase || !action) return;

  const percent = Math.max(0, Math.min(100, Math.round(state.percent)));
  if (state.percent <= 0) {
    bar.classList.add("indeterminate");
  } else {
    bar.classList.remove("indeterminate");
  }
  fill.style.width = `${percent}%`;
  pct.textContent = `${percent}%`;
  phase.textContent = state.phase;
  action.textContent = state.action;
}

/* ------------------------------------------------------------------ */
/* Actions                                                            */
/* ------------------------------------------------------------------ */

function startInstall(): void {
  state.step = "progress";
  state.percent = 0;
  state.phase = "Starting";
  state.action = "";
  render();
  sendToHost({ type: "install", launch: state.launch });
}

function closeWindow(): void {
  sendToHost({ type: "close" });
}

els.next.addEventListener("click", () => {
  switch (state.step) {
    case "welcome":
      state.step = "license";
      render();
      break;
    case "license":
      if (!state.accepted) return;
      state.step = "options";
      render();
      break;
    case "options":
      startInstall();
      break;
    case "finish":
      closeWindow();
      break;
    case "error":
      startInstall();
      break;
  }
});

els.back.addEventListener("click", () => {
  switch (state.step) {
    case "license":
      state.step = "welcome";
      render();
      break;
    case "options":
      state.step = "license";
      render();
      break;
    case "error":
      closeWindow();
      break;
  }
});

window.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !(event.target instanceof HTMLInputElement)) {
    if (!els.next.hidden && !els.next.disabled) els.next.click();
  }
  if (event.key === "Escape" && state.step === "finish") closeWindow();
});

/* ------------------------------------------------------------------ */
/* Host events                                                        */
/* ------------------------------------------------------------------ */

onHostMessage((event: HostEvent) => {
  switch (event.type) {
    case "init":
      if (typeof event.version === "string" && event.version) state.version = event.version;
      if (typeof event.installDir === "string" && event.installDir) {
        state.installDir = event.installDir;
      }
      if (state.step === "welcome" || state.step === "license" || state.step === "options") {
        render();
      }
      break;
    case "status":
      if (typeof event.text === "string" && event.text) {
        state.phase = event.text;
        if (state.step === "progress") updateProgress();
      }
      break;
    case "progress":
      state.percent = Number(event.percent) || 0;
      if (typeof event.action === "string") state.action = event.action;
      if (state.step === "progress") updateProgress();
      break;
    case "done":
      state.step = "finish";
      state.launch = Boolean(event.launch);
      state.percent = 100;
      render();
      break;
    case "failed":
      state.step = "error";
      state.error = typeof event.message === "string" ? event.message : "";
      render();
      break;
  }
});

render();
sendToHost({ type: "ready" });
