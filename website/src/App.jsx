import React, { useEffect, useMemo, useRef, useState } from "react";
import {
  AppBar,
  Box,
  Button,
  Card,
  CardContent,
  CardHeader,
  Chip,
  Container,
  Divider,
  Grid,
  IconButton,
  Stack,
  Tab,
  Tabs,
  Toolbar,
  Typography
} from "@mui/material";
import PowerSettingsNewIcon from "@mui/icons-material/PowerSettingsNew";
import TerminalIcon from "@mui/icons-material/Terminal";
import UsbIcon from "@mui/icons-material/Usb";
import DarkModeIcon from "@mui/icons-material/DarkMode";
import LightModeIcon from "@mui/icons-material/LightMode";
import SettingsIcon from "@mui/icons-material/Settings";
import TravelExploreIcon from "@mui/icons-material/TravelExplore";
import SubjectIcon from "@mui/icons-material/Subject";
import InfoOutlinedIcon from "@mui/icons-material/InfoOutlined";

import LogsCard from "./components/LogsCard.jsx";
import UsbLookupDialog from "./components/UsbLookupDialog.jsx";
import ConfigurationTab from "./tabs/ConfigurationTab.jsx";
import CaptureTab from "./tabs/CaptureTab.jsx";
import DebugTab from "./tabs/DebugTab.jsx";
import AboutTab from "./tabs/AboutTab.jsx";

import {
  EEPROM_SIZE,
  EP_IN,
  EP_OUT,
  FLAG_BOOT_CONNECTED,
  FLAG_CAPTURE_ON_BOOT,
  FLAG_REMOTE_WAKEUP,
  FLAG_SELF_POWERED,
  IFACE,
  PID,
  TLV_ATTACH_DELAY_MS,
  TLV_BCD_DEVICE,
  TLV_CAPTURE_MAX_BYTES,
  TLV_FLAGS,
  TLV_MANUFACTURER,
  TLV_MAX_POWER_MA,
  TLV_PID,
  TLV_PRODUCT,
  TLV_SERIAL,
  TLV_VID,
  VID
} from "./lib/usbsp/constants.js";
import { encodeTlv, packTlvs, parseTlvStore, u16leBytes } from "./lib/usbsp/tlv.js";
import { clampInt, hex4, parseHexInput, sanitizeHex4Input } from "./lib/usbsp/utils.js";
import { transferRawWithTimeout } from "./lib/usbsp/webusb.js";

export default function App({ colorMode = "light", onToggleColorMode = null }) {
  const deviceRef = useRef(null);
  const busyRef = useRef(false);

  const [connected, setConnected] = useState(false);
  const [busy, setBusy] = useState(false);

  const [showLogs, setShowLogs] = useState(() => {
    try {
      const v = localStorage.getItem("usbsp_showLogs");
      return v === null ? true : v === "1";
    } catch {
      return true;
    }
  });

  const [tab, setTab] = useState(0);

  const [config, setConfig] = useState({
    vid: "1209",
    pid: "0001",
    bcdDevice: "0100",
    manufacturer: "",
    product: "",
    serial: "",
    maxPowerMa: 100,
    selfPowered: false,
    remoteWakeup: false,
    bootConnected: true,
    attachDelayMs: 0,
    captureEnabledOnBoot: true,
    captureMaxBytes: 64
  });

  const [captureRunning, setCaptureRunning] = useState(false);
  const [packets, setPackets] = useState([]);

  const webVersion = import.meta.env.VITE_WEB_VERSION ?? "dev";
  const [versions, setVersions] = useState({ ch572d: null });

  const [deviceConfigSnapshot, setDeviceConfigSnapshot] = useState(null);

  const [usbLookupOpen, setUsbLookupOpen] = useState(false);

  const [eepAddr, setEepAddr] = useState(0);
  const [eepLen, setEepLen] = useState(4);
  const [raw, setRaw] = useState("");

  const [logText, setLogText] = useState("");

  const statusChip = useMemo(() => {
    if (connected) return { label: "Connected", color: "success" };
    return { label: "Disconnected", color: "error" };
  }, [connected]);

  function log(msg) {
    setLogText((prev) => prev + String(msg) + "\n");
  }

  useEffect(() => {
    try {
      localStorage.setItem("usbsp_showLogs", showLogs ? "1" : "0");
    } catch {
      // ignore
    }
  }, [showLogs]);

  async function withBusy(fn) {
    if (busyRef.current) return;
    busyRef.current = true;
    setBusy(true);
    try {
      return await fn();
    } finally {
      busyRef.current = false;
      setBusy(false);
    }
  }

  async function connect() {
    await withBusy(async () => {
      let device;
      try {
        device = await navigator.usb.requestDevice({
          filters: [{ vendorId: VID, productId: PID }]
        });
      } catch (err) {
        if (
          err?.name === "NotFoundError" ||
          err?.name === "AbortError" ||
          /no device selected/i.test(err?.message ?? "")
        ) {
          return;
        }
        throw err;
      }

      await device.open();
      if (device.configuration === null) {
        await device.selectConfiguration(1);
      }
      await device.claimInterface(IFACE);

      deviceRef.current = device;
      setConnected(true);
      log(`Connected: ${device.productName || "USB device"}`);

      try {
        const v = await getVersionsNoBusy();
        setVersions({ ch572d: v.ch572d });
      } catch (err) {
        log(`VERSIONS: ${err?.message ?? String(err)}`);
      }

      try {
        await readConfigNoBusy();
      } catch (err) {
        log(`CONFIG: auto-read failed: ${err?.message ?? String(err)}`);
      }
    });
  }

  async function disconnect() {
    await withBusy(async () => {
      const device = deviceRef.current;
      if (!device) return;
      try {
        await device.releaseInterface(IFACE);
      } catch {
        // Ignore.
        }
      try {
        await device.close();
      } catch {
        // Ignore.
      } finally {
        deviceRef.current = null;
        setConnected(false);
        setVersions({ ch572d: null });
        log("Disconnected");
      }
    });
  }

  async function transferRaw(payload) {
    const device = deviceRef.current;
    if (!device) return null;
    return await transferRawWithTimeout(device, EP_OUT, EP_IN, payload, {
      outTimeoutMs: 5000,
      inTimeoutMs: 20000,
      inLen: 64
    });
  }

  async function transfer(payload) {
    return await withBusy(async () => await transferRaw(payload));
  }

  async function eepromReadBlock(addr, len) {
    const a = clampInt(addr, 0, 255, 0);
    const l = clampInt(len, 1, 61, 1);
    const payload = new Uint8Array([0x01, a & 0xff, l & 0xff]);
    const data = await transferRaw(payload);
    if (!data) return null;
    if (data[0] !== 0x01) return null;
    const readLen = data[2] ?? 0;
    return data.slice(3, 3 + readLen);
  }

  async function readEepromAll() {
    const out = new Uint8Array(EEPROM_SIZE);
    let off = 0;
    while (off < EEPROM_SIZE) {
      const want = Math.min(61, EEPROM_SIZE - off);
      const block = await eepromReadBlock(off, want);
      if (!block) throw new Error(`EEPROM read failed at 0x${off.toString(16)}`);
      out.set(block, off);
      off += block.length;
      if (block.length === 0) throw new Error("EEPROM read returned 0 bytes");
    }
    return out;
  }

  function downloadBytes(filename, bytes, mime = "application/octet-stream") {
    const blob = new Blob([bytes], { type: mime });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  async function readEeprom() {
    const addr = clampInt(eepAddr, 0, 255, 0);
    const len = clampInt(eepLen, 1, 61, 1);
    const payload = new Uint8Array([0x01, addr & 0xff, len & 0xff]);

    const data = await transfer(payload);
    if (!data) return;
    if (data[0] !== 0x01) {
      log("EEPROM: invalid response");
      return;
    }

    const readLen = data[2];
    const bytes = Array.from(data.slice(3, 3 + readLen)).map((b) =>
      b.toString(16).padStart(2, "0")
    );
    log(`EEPROM[${addr}] len=${readLen}: ${bytes.join(" ")}`);
  }

  async function sendRaw() {
    const payload = parseHexInput(raw);
    if (!payload.length) {
      log("RAW: no bytes to send");
      return;
    }
    const data = await transfer(payload);
    if (!data) return;
    const bytes = Array.from(data).map((b) => b.toString(16).padStart(2, "0"));
    log(`RAW RX: ${bytes.join(" ")}`);
  }

  async function sendSpiText() {
    const payload = new Uint8Array([0x03]);
    const data = await transfer(payload);
    if (!data) return;
    if (data[0] !== 0x03) {
      log("SPI: invalid response");
      return;
    }
    const readLen = data[2];
    const respText = new TextDecoder().decode(data.slice(3, 3 + readLen));
    log("SPI TX: ping-pong test");
    log(`SPI RX: "${respText}"`);
  }

  useEffect(() => {
    function onDisconnect(event) {
      const device = deviceRef.current;
      if (device && event.device === device) {
        disconnect().catch(() => {});
      }
    }

    navigator.usb?.addEventListener?.("disconnect", onDisconnect);
    return () => navigator.usb?.removeEventListener?.("disconnect", onDisconnect);
  }, []);

  const disableConnect = busy || connected;
  const disableActions = busy || !connected;

  async function readConfigNoBusy() {
    if (!deviceRef.current) return;

    const eeprom = await readEepromAll();
    const records = parseTlvStore(eeprom);

    const td = new TextDecoder();
    const last = new Map();
    for (const r of records) last.set(r.type, r.value);

    const u16 = (t) => {
      const v = last.get(t);
      if (!v || v.length < 2) return null;
      return (v[0] | (v[1] << 8)) >>> 0;
    };
    const u8 = (t) => {
      const v = last.get(t);
      if (!v || v.length < 1) return null;
      return v[0] >>> 0;
    };
    const str = (t) => {
      const v = last.get(t);
      if (!v) return null;
      const nul = v.indexOf(0);
      const bytes = nul >= 0 ? v.slice(0, nul) : v;
      return td.decode(bytes);
    };

    const flags = u8(TLV_FLAGS);

    const nextConfig = {
      vid: u16(TLV_VID) != null ? hex4(u16(TLV_VID)) : "",
      pid: u16(TLV_PID) != null ? hex4(u16(TLV_PID)) : "",
      bcdDevice: u16(TLV_BCD_DEVICE) != null ? hex4(u16(TLV_BCD_DEVICE)) : "0100",
      manufacturer: str(TLV_MANUFACTURER) != null ? str(TLV_MANUFACTURER) : "",
      product: str(TLV_PRODUCT) != null ? str(TLV_PRODUCT) : "",
      serial: str(TLV_SERIAL) != null ? str(TLV_SERIAL) : "",
      maxPowerMa: u16(TLV_MAX_POWER_MA) != null ? u16(TLV_MAX_POWER_MA) : 100,
      selfPowered: flags != null ? (flags & FLAG_SELF_POWERED) !== 0 : false,
      remoteWakeup: flags != null ? (flags & FLAG_REMOTE_WAKEUP) !== 0 : false,
      bootConnected: flags != null ? (flags & FLAG_BOOT_CONNECTED) !== 0 : true,
      attachDelayMs: u16(TLV_ATTACH_DELAY_MS) != null ? u16(TLV_ATTACH_DELAY_MS) : 0,
      captureEnabledOnBoot: flags != null ? (flags & FLAG_CAPTURE_ON_BOOT) !== 0 : true,
      captureMaxBytes: u16(TLV_CAPTURE_MAX_BYTES) != null ? u16(TLV_CAPTURE_MAX_BYTES) : 64
    };

    setConfig(nextConfig);
    setDeviceConfigSnapshot(nextConfig);

    log(`CONFIG: loaded ${records.length} TLV record(s) from EEPROM`);
  }

  async function readConfig() {
    await withBusy(async () => await readConfigNoBusy());
  }

  async function writeConfig() {
    await withBusy(async () => {
      if (!deviceRef.current) return;

      const td = new TextEncoder();
      const vid16 = Number.parseInt(String(config.vid).trim().replace(/^0x/i, ""), 16);
      const pid16 = Number.parseInt(String(config.pid).trim().replace(/^0x/i, ""), 16);
      const bcd16 = Number.parseInt(String(config.bcdDevice).trim().replace(/^0x/i, ""), 16);

      const flags =
        (config.selfPowered ? FLAG_SELF_POWERED : 0) |
        (config.remoteWakeup ? FLAG_REMOTE_WAKEUP : 0) |
        (config.bootConnected ? FLAG_BOOT_CONNECTED : 0) |
        (config.captureEnabledOnBoot ? FLAG_CAPTURE_ON_BOOT : 0);

      const tlvs = [];
      if (Number.isFinite(vid16)) tlvs.push(encodeTlv(TLV_VID, u16leBytes(vid16)));
      if (Number.isFinite(pid16)) tlvs.push(encodeTlv(TLV_PID, u16leBytes(pid16)));
      if (Number.isFinite(bcd16)) tlvs.push(encodeTlv(TLV_BCD_DEVICE, u16leBytes(bcd16)));
      tlvs.push(encodeTlv(TLV_MAX_POWER_MA, u16leBytes(clampInt(config.maxPowerMa, 0, 500, 100))));
      tlvs.push(encodeTlv(TLV_FLAGS, new Uint8Array([flags & 0xff])));
      tlvs.push(encodeTlv(TLV_ATTACH_DELAY_MS, u16leBytes(clampInt(config.attachDelayMs, 0, 60000, 0))));
      tlvs.push(encodeTlv(TLV_CAPTURE_MAX_BYTES, u16leBytes(clampInt(config.captureMaxBytes, 0, 512, 64))));

      const m = td.encode(String(config.manufacturer ?? ""));
      const p = td.encode(String(config.product ?? ""));
      const s = td.encode(String(config.serial ?? ""));

      const clampStr = (bytes) => bytes.slice(0, 60);
      tlvs.push(encodeTlv(TLV_MANUFACTURER, clampStr(m)));
      tlvs.push(encodeTlv(TLV_PRODUCT, clampStr(p)));
      tlvs.push(encodeTlv(TLV_SERIAL, clampStr(s)));

      if (deviceConfigSnapshot) {
        const curNorm = {
          vid: sanitizeHex4Input(config.vid),
          pid: sanitizeHex4Input(config.pid),
          bcdDevice: sanitizeHex4Input(config.bcdDevice),
          manufacturer: String(config.manufacturer ?? ""),
          product: String(config.product ?? ""),
          serial: String(config.serial ?? ""),
          maxPowerMa: clampInt(config.maxPowerMa, 0, 500, 100),
          selfPowered: !!config.selfPowered,
          remoteWakeup: !!config.remoteWakeup,
          bootConnected: !!config.bootConnected,
          attachDelayMs: clampInt(config.attachDelayMs, 0, 60000, 0),
          captureEnabledOnBoot: !!config.captureEnabledOnBoot,
          captureMaxBytes: clampInt(config.captureMaxBytes, 0, 512, 64)
        };

        const snap = deviceConfigSnapshot;
        const same =
          curNorm.vid === sanitizeHex4Input(snap.vid) &&
          curNorm.pid === sanitizeHex4Input(snap.pid) &&
          curNorm.bcdDevice === sanitizeHex4Input(snap.bcdDevice) &&
          curNorm.manufacturer === String(snap.manufacturer ?? "") &&
          curNorm.product === String(snap.product ?? "") &&
          curNorm.serial === String(snap.serial ?? "") &&
          curNorm.maxPowerMa === clampInt(snap.maxPowerMa, 0, 500, 100) &&
          curNorm.selfPowered === !!snap.selfPowered &&
          curNorm.remoteWakeup === !!snap.remoteWakeup &&
          curNorm.bootConnected === !!snap.bootConnected &&
          curNorm.attachDelayMs === clampInt(snap.attachDelayMs, 0, 60000, 0) &&
          curNorm.captureEnabledOnBoot === !!snap.captureEnabledOnBoot &&
          curNorm.captureMaxBytes === clampInt(snap.captureMaxBytes, 0, 512, 64);

        if (same) {
          log("CONFIG: no changes (skipping write)");
          return;
        }
      }

      const batches = packTlvs(tlvs, 63); // 64B frame - 1B CMD

      let appliedBatches = 0;
      for (const batch of batches) {
        const payload = new Uint8Array(1 + batch.length);
        payload[0] = 0x04;
        payload.set(batch, 1);
        const resp = await transferRaw(payload);
        if (!resp || resp[0] !== 0x04 || resp[1] !== 0x00) {
          const st = resp ? resp[1] : null;
          const err = resp && resp.length >= 4 ? resp[3] : null;
          throw new Error(`CONFIG: write failed (status=${st ?? "N/A"} err=${err ?? "N/A"})`);
        }
        appliedBatches++;
      }

      log(`CONFIG: wrote ${tlvs.length} record(s) in ${appliedBatches} write(s)`);
      setDeviceConfigSnapshot({
        ...config,
        vid: sanitizeHex4Input(config.vid),
        pid: sanitizeHex4Input(config.pid),
        bcdDevice: sanitizeHex4Input(config.bcdDevice),
        maxPowerMa: clampInt(config.maxPowerMa, 0, 500, 100),
        attachDelayMs: clampInt(config.attachDelayMs, 0, 60000, 0),
        captureMaxBytes: clampInt(config.captureMaxBytes, 0, 512, 64)
      });
    });
  }

  async function dumpEeprom() {
    await withBusy(async () => {
      if (!deviceRef.current) return;
      const bytes = await readEepromAll();
      const ts = new Date();
      const stamp = ts
        .toISOString()
        .replaceAll(":", "")
        .replaceAll("-", "")
        .replace(".", "")
        .replace("Z", "Z");
      const name = `usbsp-eeprom-${stamp}.bin`;
      downloadBytes(name, bytes);
      log(`EEPROM: dumped ${bytes.length} bytes -> ${name}`);
    });
  }

  async function getVersionsNoBusy() {
    const resp = await transferRaw(new Uint8Array([0x10]));
    if (!resp || resp[0] !== 0x10 || resp.length < 6) {
      throw new Error("VERSIONS: invalid response");
    }
    if (resp[1] !== 0x00) {
      throw new Error("VERSIONS: device returned error");
    }
    return { ch572d: `${resp[2]}.${resp[3]}.${resp[4]}` };
  }

  async function refreshVersions() {
    await withBusy(async () => {
      if (!deviceRef.current) return;
      const v = await getVersionsNoBusy();
      setVersions({ ch572d: v.ch572d });
      log(`VERSIONS: CH572D ${v.ch572d}`);
    });
  }

  async function startCapture() {
    await withBusy(async () => {
      if (!deviceRef.current) return;
      setCaptureRunning(true);
      log("CAPTURE: start not implemented (need capture stream protocol)");
    });
  }

  async function stopCapture() {
    await withBusy(async () => {
      setCaptureRunning(false);
      log("CAPTURE: stop not implemented");
    });
  }

  return (
    <Box
      sx={{
        minHeight: "100vh",
        bgcolor: "background.default"
      }}
    >
      <AppBar position="sticky">
        <Toolbar>
          <Stack direction="row" spacing={1.25} alignItems="center" sx={{ flex: 1, minWidth: 0 }}>
            <Box
              sx={{
                width: 34,
                height: 34,
                borderRadius: 10,
                display: "grid",
                placeItems: "center",
                color: "#0b1220",
                backgroundColor: "#f1f5f9",
                border: "1px solid #d7dbe3"
              }}
            >
              <UsbIcon fontSize="small" />
            </Box>
            <Box sx={{ minWidth: 0 }}>
              <Typography variant="h6" sx={{ fontWeight: 900, lineHeight: 1.1 }}>
                USBSP
              </Typography>
            </Box>
          </Stack>

          <Stack direction="row" spacing={1} alignItems="center" useFlexGap flexWrap="wrap">
            <Button
              variant="text"
              size="small"
              startIcon={<SubjectIcon />}
              onClick={() => setShowLogs((v) => !v)}
              sx={{ display: { xs: "none", sm: "inline-flex" } }}
            >
              {showLogs ? "Hide logs" : "Show logs"}
            </Button>
            <IconButton
              size="small"
              onClick={() => onToggleColorMode?.()}
              aria-label={colorMode === "dark" ? "Switch to light theme" : "Switch to dark theme"}
              sx={{ border: "1px solid", borderColor: "divider", borderRadius: 2 }}
            >
              {colorMode === "dark" ? <LightModeIcon fontSize="small" /> : <DarkModeIcon fontSize="small" />}
            </IconButton>
            <Chip
              label={statusChip.label}
              color={statusChip.color}
              variant="filled"
              sx={{ px: 0.5 }}
            />
            <Button
              variant="contained"
              startIcon={<PowerSettingsNewIcon />}
              onClick={() => connect().catch((err) => log(err?.message ?? String(err)))}
              disabled={disableConnect}
            >
              Connect
            </Button>
            <Button
              variant="outlined"
              onClick={() => disconnect().catch((err) => log(err?.message ?? String(err)))}
              disabled={busy || !connected}
            >
              Disconnect
            </Button>
          </Stack>
        </Toolbar>
      </AppBar>

      <Container sx={{ py: { xs: 2.5, md: 4 } }}>
        <Grid container spacing={3}>
          <Grid item xs={12} md={showLogs ? 8 : 12}>
            <Card variant="outlined" sx={{ overflow: "hidden" }}>
              <CardHeader
                title="Control"
                subheader="Configure relay parameters stored in EEPROM and stream captured USB traffic"
                action={
                  <Button
                    variant="text"
                    size="small"
                    startIcon={<SubjectIcon />}
                    onClick={() => setShowLogs((v) => !v)}
                    sx={{ display: { xs: "inline-flex", sm: "none" } }}
                  >
                    {showLogs ? "Hide logs" : "Show logs"}
                  </Button>
                }
              />
              <CardContent sx={{ pt: 0 }}>
                <Tabs
                  value={tab}
                  onChange={(_, v) => setTab(v)}
                  variant="scrollable"
                  allowScrollButtonsMobile
                >
                  <Tab icon={<SettingsIcon />} iconPosition="start" label="Configuration" />
                  <Tab icon={<TravelExploreIcon />} iconPosition="start" label="Capture" />
                  <Tab icon={<TerminalIcon />} iconPosition="start" label="Debug" />
                  <Tab icon={<InfoOutlinedIcon />} iconPosition="start" label="About" />
                </Tabs>
                <Divider sx={{ my: 2 }} />

                {tab === 0 && (
                  <ConfigurationTab
                    config={config}
                    setConfig={setConfig}
                    disableActions={disableActions}
                    onReadFromDevice={() => readConfig().catch((err) => log(err?.message ?? String(err)))}
                    onWriteToDevice={() => writeConfig().catch((err) => log(err?.message ?? String(err)))}
                    onOpenUsbLookup={() => setUsbLookupOpen(true)}
                  />
                )}

                {tab === 1 && (
                  <CaptureTab
                    captureRunning={captureRunning}
                    packetsLen={packets.length}
                    disableActions={disableActions}
                    busy={busy}
                    connected={connected}
                    onStart={() => startCapture().catch((err) => log(err?.message ?? String(err)))}
                    onStop={() => stopCapture().catch((err) => log(err?.message ?? String(err)))}
                    onClearPackets={() => setPackets([])}
                  />
                )}

                {tab === 2 && (
                  <DebugTab
                    eepAddr={eepAddr}
                    setEepAddr={setEepAddr}
                    eepLen={eepLen}
                    setEepLen={setEepLen}
                    raw={raw}
                    setRaw={setRaw}
                    versions={versions}
                    webVersion={webVersion}
                    disableActions={disableActions}
                    busy={busy}
                    connected={connected}
                    captureRunning={captureRunning}
                    onDumpEeprom={() => dumpEeprom().catch((err) => log(err?.message ?? String(err)))}
                    onReadEeprom={() => readEeprom().catch((err) => log(err?.message ?? String(err)))}
                    onRefreshVersions={() => refreshVersions().catch((err) => log(err?.message ?? String(err)))}
                    onSendRaw={() => sendRaw().catch((err) => log(err?.message ?? String(err)))}
                    onSpiTest={() => sendSpiText().catch((err) => log(err?.message ?? String(err)))}
                  />
                )}

                {tab === 3 && (
                  <AboutTab webVersion={webVersion} versions={versions} />
                )}
              </CardContent>
            </Card>
          </Grid>

          {showLogs && (
            <Grid item xs={12} md={4}>
              <LogsCard logText={logText} onClear={() => setLogText("")} />
            </Grid>
          )}
        </Grid>
      </Container>

      <UsbLookupDialog
        open={usbLookupOpen}
        onClose={() => setUsbLookupOpen(false)}
        onApply={(sel) => {
          setConfig((prev) => {
            const patch = {
              vid: sel?.vid ?? prev.vid,
              pid: sel?.pid ?? prev.pid
            };
            if (typeof sel?.manufacturer === "string") patch.manufacturer = sel.manufacturer;
            if (typeof sel?.product === "string") patch.product = sel.product;
            return { ...prev, ...patch };
          });
        }}
      />
    </Box>
  );
}
