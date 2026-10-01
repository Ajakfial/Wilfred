// Shared shapes for the Wilfred overlay protocol (mirrors
// overlay_results_json / overlay_preview_json in src/ui/web_ui.cpp).
// The assist fields (correction/ghost/candidates) are served identically by
// the Windows (WebView2), macOS (WKWebView) and Linux (WebKitGTK/X11)
// hosts — the web UI must treat them as optional for back-compat.

export interface ActionItem {
  id: string;
  label: string;
}

export interface ResultItem {
  title?: string;
  subtitle?: string;
  path?: string;
  kind?: string;
  action?: string;
  category?: string;
  payload?: string;
  plugin?: string;
  actions?: ActionItem[];
  meter?: number;
  icon?: string;
}

export interface RowEntry {
  item: ResultItem;
  /** Index into the original items array (sent back on submit/preview). */
  index: number;
}

export interface PreviewMsg {
  type: "preview";
  title?: string;
  kind?: string;
  size?: string;
  modified?: string;
  text?: string;
  image?: string;
  error?: string;
}

export interface ResultsMsg {
  type: "results";
  items?: ResultItem[];
  habits?: string[];
  /** "Did you mean" rewrite. Empty/undefined = no confident correction. */
  correction?: string;
  /** Inline ghost completion extending the current query. */
  ghost?: string;
  /** Top-N autocomplete candidates for the dropdown. */
  candidates?: string[];
  /** Echo of the query that produced these results (stale-race guard). */
  query?: string;
  id?: number;
}

export type NativeInMsg =
  | ShowMsg
  | ConfigMsg
  | { type: "hide" }
  | ResultsMsg
  | PreviewMsg
  | { type: string; [k: string]: unknown };

export interface ShowMsg {
  type: "show";
  transparent?: boolean;
  opacity?: number;
  blur?: boolean;
}

export interface ConfigMsg {
  type: "config";
  transparent?: boolean;
  opacity?: number;
  blur?: boolean;
}

export interface NativeOutMsg {
  type: "ready" | "query" | "submit" | "preview" | "resize" | "hidden";
  q?: string;
  id?: number;
  index?: number;
  action?: string;
  width?: number;
  height?: number;
}
