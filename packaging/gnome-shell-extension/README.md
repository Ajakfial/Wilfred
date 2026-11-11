# Wilfred GNOME Shell extension

GNOME does not implement the `wlr-layer-shell` protocol, so Wilfred's overlay
cannot be anchored as a native layer-shell surface there. This extension is the
GNOME-side fallback: it keeps any Wilfred window above other windows and out of
the taskbar/pager, approximating layer-shell "top + skip-taskbar" behavior.

## Install

```sh
mkdir -p ~/.local/share/gnome-shell/extensions
cp -r wilfred-layer-shell@wilfred ~/.local/share/gnome-shell/extensions/
gnome-extensions enable wilfred-layer-shell@wilfred
```

Then restart GNOME Shell (Alt+F2, `r` on X11; log out/in on Wayland).