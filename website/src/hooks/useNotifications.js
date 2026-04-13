import { useCallback, useState } from "react";

function formatErrorMessage(err) {
  const message = err?.message ?? String(err);
  if (/claiminterface/i.test(message) || /unable to claim interface/i.test(message)) {
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
