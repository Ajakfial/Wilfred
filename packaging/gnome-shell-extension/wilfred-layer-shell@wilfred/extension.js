import * as Main from 'resource:///org/gnome/shell/ui/main.js';

function isWilfred(window) {
  if (!window) return false;
  try {
    const wmClass = (window.get_wm_class() || '').toLowerCase();
    if (wmClass.includes('wilfred')) return true;
  } catch (e) {}
  try {
    const title = (window.get_title() || '').toLowerCase();
    if (title.includes('wilfred')) return true;
  } catch (e) {}
  return false;
}

function promote(window) {
  if (!isWilfred(window)) return;
  try { window.make_above(); } catch (e) {}
  try { window.set_skip_taskbar(true); } catch (e) {}
  try { window.set_skip_pager(true); } catch (e) {}
}

export default class WilfredLayerShellExtension {
  enable() {
    this._createdId = global.display.connect('window-created', (_display, window) => promote(window));
    try {
      global.get_window_actors().forEach(actor => promote(actor.get_meta_window()));
    } catch (e) {}
  }

  disable() {
    if (this._createdId) {
      try { global.display.disconnect(this._createdId); } catch (e) {}
      this._createdId = null;
    }
  }
}