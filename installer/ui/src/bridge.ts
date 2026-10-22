/**
 * Typed bridge between the wizard and the native WebView2 host.
 *
 * Host -> UI events are plain JSON objects delivered by
 * window.chrome.webview. UI -> host messages are objects handed to
 * postMessage; the C++ side reads them with get_WebMessageAsJson.
 *
 * When the page is opened in a normal browser (npm run preview / opening
 * dist/index.html directly) a small mock drives the same state machine so
 * the UI can be designed without building the host.
 */

export type InitEvent = { type: "init"; version: string; installDir: string };
export type StatusEvent = { type: "status"; text: string };
export type ProgressEvent = { type: "progress"; percent: number; action?: string };
export type DoneEvent = { type: "done"; success: boolean; launch: boolean };
export type FailedEvent = { type: "failed"; message: string; code: number };

export type HostEvent = InitEvent | StatusEvent | ProgressEvent | DoneEvent | FailedEvent;

type WebViewBridge = {
  postMessage: (message: unknown) => void;
  addEventListener: (type: "message", handler: (event: { data: unknown }) => void) => void;
};

function webview(): WebViewBridge | undefined {
  return (window as Window & { chrome?: { webview?: WebViewBridge } }).chrome?.webview;
}

export function hostAvailable(): boolean {
  return Boolean(webview());
}

let mockEmit: ((event: HostEvent) => void) | undefined;
let mockTimers: number[] = [];

export function sendToHost(message: Record<string, unknown>): void {
  const host = webview();
  if (host) {
    host.postMessage(message);
    return;
  }
  mockSend(message);
}

export function onHostMessage(handler: (event: HostEvent) => void): void {
  const host = webview();
  if (!host) {
    mockEmit = handler;
    mockStart();
    return;
  }
  host.addEventListener("message", (event) => {
    const data = event.data;
    if (!data || typeof data !== "object") return;
    const type = (data as { type?: unknown }).type;
    if (typeof type === "string") handler(data as HostEvent);
  });
}

/* ------------------------------------------------------------------ */
/* Browser preview mock                                                */
/* ------------------------------------------------------------------ */

function mockStart(): void {
  window.setTimeout(() => {
    mockEmit?.({
      type: "init",
      version: "preview",
      installDir: "C:\\Users\\you\\AppData\\Local\\Wilfred",
    });
  }, 200);
}

function mockSend(message: Record<string, unknown>): void {
  if (message.type === "install") {
    const actions = [
      "Preparing installation",
      "Copying program files",
      "Creating shortcuts",
      "Registering with Windows",
      "Finishing up",
    ];
    let percent = 0;
    let action = 0;
    mockTimers.push(
      window.setInterval(() => {
        percent = Math.min(100, percent + 3 + Math.random() * 4);
        if (percent > (action + 1) * 20 && action < actions.length - 1) action += 1;
        mockEmit?.({ type: "progress", percent, action: actions[action] });
        if (percent >= 100) {
          mockTimers.forEach((timer) => window.clearInterval(timer));
          mockTimers = [];
          mockEmit?.({
            type: "done",
            success: true,
            launch: message.launch === true,
          });
        }
      }, 140),
    );
  }
  if (message.type === "close") {
    const toast = document.createElement("div");
    toast.textContent = "Preview mode: the real setup closes here.";
    Object.assign(toast.style, {
      position: "fixed",
      inset: "auto 0 0 0",
      padding: "12px",
      background: "#7f8cff",
      color: "#101223",
      textAlign: "center",
      font: "600 13px system-ui",
    });
    document.body.append(toast);
  }
}
