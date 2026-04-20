import React, { useMemo, useState } from "react";
import {
  Alert,
  AppBar,
  Box,
  Button,
  Card,
  CardContent,
  CardHeader,
  Chip,
  Container,
  Divider,
  IconButton,
  Link,
  Snackbar,
  Stack,
  Tab,
  Tabs,
  Toolbar,
  Typography
} from "@mui/material";
import PowerSettingsNewIcon from "@mui/icons-material/PowerSettingsNew";
import DarkModeIcon from "@mui/icons-material/DarkMode";
import LightModeIcon from "@mui/icons-material/LightMode";
import SettingsIcon from "@mui/icons-material/Settings";
import TravelExploreIcon from "@mui/icons-material/TravelExplore";
import TerminalIcon from "@mui/icons-material/Terminal";
import InfoOutlinedIcon from "@mui/icons-material/InfoOutlined";

import UsbLookupDialog from "./components/UsbLookupDialog.jsx";
import ConfigurationTab from "./tabs/ConfigurationTab.jsx";
import CaptureTab from "./tabs/CaptureTab.jsx";
import DebugTab from "./tabs/DebugTab.jsx";
import AboutTab from "./tabs/AboutTab.jsx";
import { useNotifications } from "./hooks/useNotifications.js";
import { useUsbspApp } from "./hooks/useUsbspApp.js";

export default function App({ colorMode = "light", onToggleColorMode = null }) {
  const [tab, setTab] = useState(0);
  const notifications = useNotifications();
  const app = useUsbspApp(notifications);
  const lockedTabSx = app.captureRunning
    ? {
        opacity: 0.45,
        color: "text.disabled"
      }
    : null;

  const handleTabChange = (_, value) => {
    if (app.captureRunning && value !== 1) {
      notifications.notify("error", "Stop capture before leaving this tab.");
      return;
    }
    setTab(value);
  };

  const webVersion = import.meta.env.VITE_WEB_VERSION ?? "dev";
  const statusChip = useMemo(() => {
    if (app.connected) return { label: "Connected", color: "success" };
    return { label: "Disconnected", color: "error" };
  }, [app.connected]);

  return (
    <Box sx={{ minHeight: "100vh", bgcolor: "background.default" }}>
      <AppBar position="sticky">
        <Toolbar sx={{ minHeight: 68, gap: 2 }}>
          <Stack direction="row" spacing={1.25} alignItems="center" sx={{ flex: 1, minWidth: 0 }}>
            <Box
              sx={{
                width: 28,
                height: 28,
                display: "grid",
                placeItems: "center"
              }}
            >
              <Box
                component="img"
                src="/favicon.svg"
                alt="USBsp icon"
                sx={{ width: 28, height: 28, display: "block" }}
              />
            </Box>
            <Typography variant="h6" sx={{ fontWeight: 900, lineHeight: 1.1, letterSpacing: 0.2 }}>
              USBsp
            </Typography>
          </Stack>

          <Stack direction="row" spacing={0.75} alignItems="center" useFlexGap flexWrap="wrap">
            <IconButton
              size="small"
              onClick={() => onToggleColorMode?.()}
              aria-label={colorMode === "dark" ? "Switch to light theme" : "Switch to dark theme"}
              sx={{ border: "1px solid", borderColor: "divider", borderRadius: 2 }}
            >
              {colorMode === "dark" ? <LightModeIcon fontSize="small" /> : <DarkModeIcon fontSize="small" />}
            </IconButton>
            <Chip label={statusChip.label} color={statusChip.color} variant="filled" size="small" sx={{ px: 0.25 }} />
            <Button
              variant="contained"
              color={app.connected ? "error" : "primary"}
              startIcon={<PowerSettingsNewIcon />}
              onClick={app.connected ? app.disconnect : app.connect}
              disabled={app.disableConnect}
            >
              {app.connected ? "Disconnect" : "Connect"}
            </Button>
          </Stack>
        </Toolbar>
      </AppBar>

      <Container sx={{ py: { xs: 2.5, md: 4 } }}>
        {!app.webUsbSupported ? (
          <Alert severity="warning" sx={{ mb: 2 }}>
            {app.webUsbUnavailableReason}
          </Alert>
        ) : null}

        <Card variant="outlined" sx={{ overflow: "hidden" }}>
          <CardHeader
            title="Device Control"
            subheader="Configure your device and capture USB traffic"
          />
          <CardContent sx={{ pt: 0 }}>
            <Tabs value={tab} onChange={handleTabChange} variant="scrollable" allowScrollButtonsMobile>
              <Tab icon={<SettingsIcon />} iconPosition="start" label="Configuration" sx={lockedTabSx} />
              <Tab icon={<TravelExploreIcon />} iconPosition="start" label="Capture" />
              <Tab icon={<TerminalIcon />} iconPosition="start" label="Advanced" sx={lockedTabSx} />
              <Tab icon={<InfoOutlinedIcon />} iconPosition="start" label="About" sx={lockedTabSx} />
            </Tabs>
            <Divider sx={{ my: 2 }} />

            {tab === 0 && (
              <ConfigurationTab
                config={app.config}
                setConfig={app.setConfig}
                disableActions={app.disableActions}
                onLoadConnectedUsb={app.loadConnectedUsbConfig}
                onReadFromDevice={app.readConfig}
                onWriteToDevice={app.writeConfig}
                onOpenUsbLookup={() => app.setUsbLookupOpen(true)}
              />
            )}

            {tab === 1 && (
              <CaptureTab
                captureRunning={app.captureRunning}
                packets={app.packets}
                disableActions={app.disableActions}
                busy={app.busy}
                connected={app.connected}
                onStart={app.startCapture}
                onStop={app.stopCapture}
                onClearPackets={app.clearPackets}
                onSaveCapture={app.saveCapture}
              />
            )}

            {tab === 2 && (
              <DebugTab
                eepAddr={app.eepAddr}
                setEepAddr={app.setEepAddr}
                eepLen={app.eepLen}
                setEepLen={app.setEepLen}
                raw={app.raw}
                setRaw={app.setRaw}
                rawError={app.rawError}
                versions={app.versions}
                webVersion={webVersion}
                disableActions={app.disableActions}
                onDumpEeprom={app.dumpEeprom}
                onReadEeprom={app.readEeprom}
                onRefreshVersions={app.refreshVersions}
                onSendRaw={app.sendRaw}
                onSpiTest={app.spiTest}
                onResetCh32={app.resetCh32}
                eepromReadResult={app.eepromReadResult}
                rawResponse={app.rawResponse}
                spiResponse={app.spiResponse}
              />
            )}

            {tab === 3 && <AboutTab webVersion={webVersion} versions={app.versions} />}
          </CardContent>
        </Card>
      </Container>

      <UsbLookupDialog
        open={app.usbLookupOpen}
        onClose={() => app.setUsbLookupOpen(false)}
        onApply={(selection) => {
          app.setConfig((prev) => {
            const patch = {
              vid: selection?.vid ?? prev.vid,
              pid: selection?.pid ?? prev.pid
            };
            if (selection?.manufacturer !== undefined) patch.manufacturer = selection.manufacturer ?? "";
            if (selection?.product !== undefined) patch.product = selection.product ?? "";
            if (selection?.serial !== undefined) patch.serial = selection.serial ?? "";
            return { ...prev, ...patch };
          });
        }}
      />

      <Snackbar
        open={notifications.alert.open}
        autoHideDuration={5000}
        onClose={notifications.closeAlert}
      >
        <Alert onClose={notifications.closeAlert} severity={notifications.alert.severity} sx={{ width: "100%" }}>
          {notifications.alert.message}
          {notifications.alert.link ? (
            <>
              {" "}
              {notifications.alert.link.prefix ?? ""}
              <Link href={notifications.alert.link.href} target="_blank" rel="noreferrer" color="inherit" underline="always">
                {notifications.alert.link.label}
              </Link>
            </>
          ) : null}
        </Alert>
      </Snackbar>
    </Box>
  );
}
