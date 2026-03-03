import React, { useEffect, useMemo, useState } from "react";
import { createRoot } from "react-dom/client";
import { CssBaseline, ThemeProvider } from "@mui/material";
import App from "./App.jsx";
import "./style.css";
import { buildTheme } from "./theme.js";

function getInitialMode() {
  try {
    const saved = localStorage.getItem("usbsp_colorMode");
    if (saved === "light" || saved === "dark") return saved;
  } catch {
    // ignore
  }
  return window.matchMedia?.("(prefers-color-scheme: dark)")?.matches ? "dark" : "light";
}

function Root() {
  const [mode, setMode] = useState(getInitialMode);

  useEffect(() => {
    try {
      localStorage.setItem("usbsp_colorMode", mode);
    } catch {
      // ignore
    }

    // Ensure browser-provided UI (notably scrollbars) tracks the selected theme.
    // `CssBaseline enableColorScheme` sets this on body, but many browsers key off `html`.
    try {
      document.documentElement.style.colorScheme = mode;
    } catch {
      // ignore
    }
  }, [mode]);

  const theme = useMemo(() => buildTheme(mode), [mode]);

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline enableColorScheme />
      <App
        colorMode={mode}
        onToggleColorMode={() => setMode((m) => (m === "dark" ? "light" : "dark"))}
      />
    </ThemeProvider>
  );
}

createRoot(document.getElementById("app")).render(
  <React.StrictMode>
    <Root />
  </React.StrictMode>
);
