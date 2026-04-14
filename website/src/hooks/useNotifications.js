import { useCallback, useState } from "react";

function isWindowsBrowser() {
  if (typeof navigator === "undefined") {
    return false;
  }

  const platform = navigator.userAgentData?.platform ?? navigator.platform ?? "";
  const userAgent = navigator.userAgent ?? "";
  return /windows/i.test(platform) || /windows/i.test(userAgent);
}

function formatErrorMessage(err) {
  const message = err?.message ?? String(err);
  if (/claiminterface/i.test(message) || /unable to claim interface/i.test(message)) {
    if (isWindowsBrowser()) {
      return "Unable to claim the USB interface. On Windows, install WinUSB for the usbsp WebUSB interface with Zadig: https://zadig.akeo.ie/";
    }
    return "Unable to claim the USB interface. Close other browser tabs or apps using this device and try again.";
  }
  return message;
}

export function useNotifications() {
  const [alert, setAlert] = useState({
    open: false,
    severity: "info",
    message: ""
  });

  const log = useCallback((message) => {
    console.log(String(message));
  }, []);

  const notify = useCallback((severity, message) => {
    setAlert({ open: true, severity, message: String(message) });
  }, []);

  const closeAlert = useCallback((_, reason) => {
    if (reason === "clickaway") return;
    setAlert((prev) => ({ ...prev, open: false }));
  }, []);

  const reportError = useCallback(
    (err, fallback = "Operation failed") => {
      const message = formatErrorMessage(err);
      log(message);
      notify("error", message || fallback);
    },
    [log, notify]
  );

  return {
    alert,
    closeAlert,
    log,
    notify,
    reportError
  };
}
