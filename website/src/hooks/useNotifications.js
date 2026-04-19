import { useCallback, useState } from "react";

export const KNOWN_ISSUES_URL = "https://github.com/Ismael034/usbsp/wiki/Common-known-issues";

function formatErrorMessage(err) {
  const message = err?.message ?? String(err);
  if (/claiminterface/i.test(message) || /unable to claim interface/i.test(message)) {
    return "Unable to claim the USB interface.";
  }
  return message;
}

function shouldLinkKnownIssues(message) {
  return /claiminterface|unable to claim interface|webusb|requestdevice|no device selected|access denied|permission denied|failed to open usb/i.test(
    message ?? ""
  );
}

export function useNotifications() {
  const [alert, setAlert] = useState({
    open: false,
    severity: "info",
    message: "",
    link: null
  });

  const log = useCallback((message) => {
    console.log(String(message));
  }, []);

  const notify = useCallback((severity, message, link = null) => {
    setAlert({ open: true, severity, message: String(message), link });
  }, []);

  const closeAlert = useCallback((_, reason) => {
    if (reason === "clickaway") return;
    setAlert((prev) => ({ ...prev, open: false }));
  }, []);

  const reportError = useCallback(
    (err, fallback = "Operation failed") => {
      const message = formatErrorMessage(err);
      log(message);
      const text = message || fallback;
      notify(
        "error",
        text,
        shouldLinkKnownIssues(text)
          ? {
              prefix: "If this keeps happening check ",
              label: "Common known issues",
              href: KNOWN_ISSUES_URL
            }
          : null
      );
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
