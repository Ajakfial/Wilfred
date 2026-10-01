import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { App } from "./App";
import { hasNativeHost } from "./protocol";

const rootEl = document.getElementById("root");
if (!rootEl) throw new Error("overlay root element missing");

// Plain-browser preview (no native host): paint a wallpaper behind the glass
// so blur, borders and shadows can be judged. Hosts stay fully transparent.
if (!hasNativeHost()) document.documentElement.classList.add("preview");

createRoot(rootEl).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
