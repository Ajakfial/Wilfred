// Shared shapes for the Wilfred overlay protocol (mirrors
// overlay_results_json / overlay_preview_json in src/ui/web_ui.cpp).

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

export type NativeInMsg =
  | { type: "show" }
  | { type: "hide" }
  | { type: "results"; items?: ResultItem[] }
  | PreviewMsg
  | { type: string; [k: string]: unknown };
