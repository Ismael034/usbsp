import { useCallback, useEffect, useMemo, useRef, useState } from "react";

import {
  EEPROM_SIZE,
  EP_IN,
  EP_OUT,
  FLAG_BOOT_CONNECTED,
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
} from "../lib/usbsp/constants.js";
import { encodeTlv, packTlvs, parseTlvStore, u16leBytes } from "../lib/usbsp/tlv.js";
import { clampInt, hex4, parseHexInput, sanitizeHex4Input, validateRawHexInput } from "../lib/usbsp/utils.js";
import { transferRawWithTimeout } from "../lib/usbsp/webusb.js";

const CMD_EEPROM_READ = 0x01;
const CMD_SPI_PINGPONG = 0x03;
const CMD_EEPROM_WRITE = 0x04;
const CMD_CH32_RESET = 0x05;
const CMD_GET_VERSIONS = 0x10;
const CMD_GET_ACTIVE_USB_INFO = 0x11;
const CMD_CAPTURE_POLL = 0x20;
const USB_CONNECT_STEP_TIMEOUT_MS = 10000;
const ACTIVE_USB_INFO_MAX_BYTES = EEPROM_SIZE;
const ACTIVE_USB_INFO_CHUNK_BYTES = 58;

async function withPromiseTimeout(promise, timeoutMs, label) {
  const ms = Number.isFinite(timeoutMs) ? timeoutMs : 0;
  if (ms <= 0) return await promise;

  let timeoutId = null;
  const timeout = new Promise((_, reject) => {
    timeoutId = setTimeout(() => reject(new Error(`${label}: timeout after ${ms}ms`)), ms);
  });

  try {
    return await Promise.race([promise, timeout]);
  } finally {
    if (timeoutId) clearTimeout(timeoutId);
  }
}

function getWebUsbSupport() {
  if (typeof navigator === "undefined") {
    return {
      supported: false,
      reason: "WebUSB is not available in this environment."
    };
  }

  if (navigator.usb) {
    return {
      supported: true,
      reason: ""
    };
  }

  if (typeof window !== "undefined" && window.isSecureContext === false) {
    return {
      supported: false,
      reason: "WebUSB requires a secure context. Open this site over HTTPS or localhost."
    };
  }

  return {
    supported: false,
    reason: "WebUSB is not supported by this browser. Use a Chromium-based desktop browser such as Chrome or Edge."
  };
}

function normalizeConfig(config) {
  return {
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
    captureMaxBytes: clampInt(config.captureMaxBytes, 0, 512, 64)
  };
}

function bytesToHex(bytes) {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join(" ");
}

function trimTrailingZeroes(bytes) {
  let end = bytes.length;
  while (end > 0 && bytes[end - 1] === 0) {
    end -= 1;
  }
  return bytes.slice(0, end);
}

function summarizePayload(direction, endpoint, payload) {
  return payload.length ? "Raw USB packet" : "-";
}

function decodeCaptureFrame(frame, packetIndexRef) {
  if (!frame?.length) return { packets: [], dropped: 0 };

  const payload = new Uint8Array(frame);
  if (!Array.from(payload).some((byte) => byte !== 0)) {
    return { packets: [], dropped: 0 };
  }

  if (payload[0] !== 0xc2) {
    const nextIndex = packetIndexRef.current + 1;
    packetIndexRef.current = nextIndex;
    const trimmed = trimTrailingZeroes(payload);
    return {
      dropped: 0,
      packets: [
        {
          id: nextIndex,
          seq: nextIndex,
          direction: "?",
          endpoint: "-",
          length: trimmed.length,
          summary: "Raw capture frame",
          hex: bytesToHex(trimmed),
          data: Array.from(trimmed)
        }
      ]
    };
  }

  const seq = payload[1] ?? 0;
  const dropped = payload[2] ?? 0;
  const count = payload[3] ?? 0;
  const packets = [];
  let cursor = 4;
  let malformed = false;

  for (let index = 0; index < count; index += 1) {
    if (cursor + 3 > payload.length) {
      malformed = true;
      break;
    }

    const meta = payload[cursor];
    const capturedLength = payload[cursor + 1];
    const originalLength = payload[cursor + 2];
    if (capturedLength > 57 || cursor + 3 + capturedLength > payload.length) {
      malformed = true;
      break;
    }

    const packetData = payload.slice(cursor + 3, cursor + 3 + capturedLength);
    const nextIndex = packetIndexRef.current + 1;
    packetIndexRef.current = nextIndex;
    const direction = (meta & 0x80) !== 0 ? "IN" : "OUT";
    const endpoint = meta & 0x7f;

    packets.push({
      id: nextIndex,
      seq,
      direction,
      endpoint,
      length: Math.max(originalLength, capturedLength),
      capturedLength,
      originalLength: Math.max(originalLength, capturedLength),
      truncated: capturedLength < originalLength,
      summary: summarizePayload(direction, endpoint, packetData),
      hex: bytesToHex(packetData),
      data: Array.from(packetData)
    });

    cursor += 3 + capturedLength;
  }

  if (packets.length === 0 && (count > 0 || malformed)) {
    const nextIndex = packetIndexRef.current + 1;
    packetIndexRef.current = nextIndex;
    const trimmed = trimTrailingZeroes(payload.slice(4));
    return {
      dropped,
      packets: [
        {
          id: nextIndex,
          seq,
          direction: "?",
          endpoint: "-",
          length: trimmed.length,
          capturedLength: trimmed.length,
          originalLength: trimmed.length,
          truncated: false,
          summary: malformed ? "Malformed capture frame" : "Raw capture frame",
          hex: bytesToHex(trimmed),
          data: Array.from(trimmed)
        }
      ]
    };
  }

  return { packets, dropped };
}

function hasCaptureContent(frame) {
  if (!frame?.length) return false;

  const payload = new Uint8Array(frame);
  if (!Array.from(payload).some((byte) => byte !== 0)) {
    return false;
  }

  if (payload[0] !== 0xc2) {
    return trimTrailingZeroes(payload).length > 0;
  }

  const dropped = payload[2] ?? 0;
  const count = payload[3] ?? 0;
  return dropped > 0 || count > 0;
}

export function useUsbspApp({ log, notify, reportError }) {
  const deviceRef = useRef(null);
  const busyRef = useRef(false);
  const captureLoopRef = useRef(false);
  const captureStopRequestedRef = useRef(false);
  const captureControlBusyRef = useRef(false);
  const packetIndexRef = useRef(0);
  const capturePendingPacketsRef = useRef([]);
  const capturePendingDroppedRef = useRef(0);
  const captureLastFlushMsRef = useRef(0);
  const captureLastDropLogMsRef = useRef(0);

  const [connected, setConnected] = useState(false);
  const [busy, setBusy] = useState(false);
  const [config, setConfig] = useState({
    vid: "",
    pid: "",
    bcdDevice: "",
    manufacturer: "",
    product: "",
    serial: "",
    maxPowerMa: 100,
    selfPowered: false,
    remoteWakeup: false,
    bootConnected: true,
    attachDelayMs: 0,
    captureMaxBytes: 64
  });
  const [versions, setVersions] = useState({ ch572d: null, ch32v203: null });
  const [deviceConfigSnapshot, setDeviceConfigSnapshot] = useState(null);
  const [usbLookupOpen, setUsbLookupOpen] = useState(false);
  const [eepAddr, setEepAddr] = useState(0);
  const [eepLen, setEepLen] = useState(4);
  const [raw, setRaw] = useState("");
  const [captureRunning, setCaptureRunning] = useState(false);
  const [packets, setPackets] = useState([]);
  const [captureDroppedTotal, setCaptureDroppedTotal] = useState(0);
  const [eepromReadResult, setEepromReadResult] = useState("");
  const [rawResponse, setRawResponse] = useState("");
  const [spiResponse, setSpiResponse] = useState("");
  const webUsbSupport = getWebUsbSupport();

  const rawError = useMemo(() => validateRawHexInput(raw), [raw]);

  const withBusy = useCallback(async (fn) => {
    if (busyRef.current) return;
    busyRef.current = true;
    setBusy(true);
    try {
      return await fn();
    } finally {
      busyRef.current = false;
      setBusy(false);
    }
  }, []);

  const transferRaw = useCallback(async (payload, opts = {}) => {
    const device = deviceRef.current;
    if (!device) return null;
    return await transferRawWithTimeout(device, EP_OUT, EP_IN, payload, {
      outTimeoutMs: 5000,
      inLen: 64,
      ...opts
    });
  }, []);

  const flushCapturedPackets = useCallback((force = false) => {
    const pending = capturePendingPacketsRef.current;
    if (!pending.length) return;

    const now = Date.now();
    if (!force && pending.length < 24 && (now - captureLastFlushMsRef.current) < 24) {
      return;
    }

    capturePendingPacketsRef.current = [];
    captureLastFlushMsRef.current = now;
    setPackets((prev) => [...prev, ...pending]);
  }, []);

  const flushDroppedCaptureLog = useCallback((force = false) => {
    const dropped = capturePendingDroppedRef.current;
    if (!dropped) return;

    const now = Date.now();
    if (!force && (now - captureLastDropLogMsRef.current) < 200) {
      return;
    }

    capturePendingDroppedRef.current = 0;
    captureLastDropLogMsRef.current = now;
    log(`CAPTURE: dropped ${dropped} packet(s) on CH32`);
  }, [log]);

  const eepromReadBlock = useCallback(
    async (addr, len) => {
      const a = clampInt(addr, 0, 255, 0);
      const l = clampInt(len, 1, 61, 1);
      const payload = new Uint8Array([CMD_EEPROM_READ, a & 0xff, l & 0xff]);
      const data = await transferRaw(payload);
      if (!data || data[0] !== CMD_EEPROM_READ) return null;
      const readLen = data[2] ?? 0;
      return data.slice(3, 3 + readLen);
    },
    [transferRaw]
  );

  const readEepromAll = useCallback(async () => {
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
  }, [eepromReadBlock]);

  const downloadBytes = useCallback((filename, bytes, mime = "application/octet-stream") => {
    const blob = new Blob([bytes], { type: mime });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = filename;
    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }, []);

  const parseConfigFromEeprom = useCallback((eeprom) => {
    const records = parseTlvStore(eeprom);
    const decoder = new TextDecoder();
    const latest = new Map();
    for (const record of records) latest.set(record.type, record.value);

    const u16 = (type) => {
      const value = latest.get(type);
      if (!value || value.length < 2) return null;
      return (value[0] | (value[1] << 8)) >>> 0;
    };
    const u8 = (type) => {
      const value = latest.get(type);
      if (!value || value.length < 1) return null;
      return value[0] >>> 0;
    };
    const str = (type) => {
      const value = latest.get(type);
      if (!value) return null;
      const nul = value.indexOf(0);
      const bytes = nul >= 0 ? value.slice(0, nul) : value;
      return decoder.decode(bytes);
    };

    const flags = u8(TLV_FLAGS);
    return {
      records,
      config: {
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
        captureMaxBytes: u16(TLV_CAPTURE_MAX_BYTES) != null ? u16(TLV_CAPTURE_MAX_BYTES) : 64
      }
    };
  }, []);

  const readConfigNoBusy = useCallback(async () => {
    if (!deviceRef.current) return;
    const eeprom = await readEepromAll();
    const parsed = parseConfigFromEeprom(eeprom);
    setConfig(parsed.config);
    setDeviceConfigSnapshot(parsed.config);
    log(`CONFIG: loaded ${parsed.records.length} TLV record(s) from EEPROM`);
    return parsed.config;
  }, [log, parseConfigFromEeprom, readEepromAll]);

  const readConfig = useCallback(async () => {
    try {
      await withBusy(async () => await readConfigNoBusy());
      notify("success", "Saved settings loaded.");
    } catch (err) {
      reportError(err, "Failed to read configuration.");
    }
  }, [notify, readConfigNoBusy, reportError, withBusy]);

  const loadConnectedUsbConfig = useCallback(async () => {
    try {
      if (!deviceRef.current) return;

      const chunks = [];
      let expectedTotalLen = null;
      let offset = 0;

      while (expectedTotalLen == null || offset < expectedTotalLen) {
        const remaining = expectedTotalLen == null ? ACTIVE_USB_INFO_CHUNK_BYTES : expectedTotalLen - offset;
        const requestLen = Math.min(ACTIVE_USB_INFO_CHUNK_BYTES, remaining);
        const resp = await transferRaw(
          new Uint8Array([
            CMD_GET_ACTIVE_USB_INFO,
            offset & 0xff,
            (offset >> 8) & 0xff,
            requestLen & 0xff
          ]),
          { inLen: 64 }
        );

        if (!resp || resp[0] !== CMD_GET_ACTIVE_USB_INFO || resp.length < 5) {
          throw new Error("USB info: invalid response");
        }
        if (resp[1] !== 0x00) {
          throw new Error(`USB info read failed rc=${resp[1]} offset=${offset} req=${requestLen}`);
        }

        const reportedTotalLen = (resp[2] | (resp[3] << 8)) >>> 0;
        if (reportedTotalLen > ACTIVE_USB_INFO_MAX_BYTES) {
          throw new Error(`USB info: reported length ${reportedTotalLen} exceeds ${ACTIVE_USB_INFO_MAX_BYTES}`);
        }
        if (expectedTotalLen == null) {
          expectedTotalLen = reportedTotalLen;
        } else if (reportedTotalLen !== expectedTotalLen) {
          throw new Error(`USB info: length changed from ${expectedTotalLen} to ${reportedTotalLen} at offset ${offset}`);
        }

        const reportedCopyLen = resp[4] ?? 0;
        const availableLen = Math.max(0, resp.length - 5);
        const remainingLen = expectedTotalLen - offset;
        if (reportedCopyLen > availableLen) {
          throw new Error(`USB info: response says ${reportedCopyLen} bytes but only ${availableLen} arrived`);
        }

        const copyLen = Math.min(reportedCopyLen, requestLen, remainingLen);
        if (reportedCopyLen !== copyLen) {
          log(`USB info: clamped chunk len=${reportedCopyLen} to ${copyLen} at offset=${offset} total=${expectedTotalLen}`);
        }

        const chunk = resp.slice(5, 5 + copyLen);
        chunks.push(chunk);
        offset += chunk.length;

        if (copyLen === 0 && offset < expectedTotalLen) {
          throw new Error(`USB info: short read at offset ${offset} of ${expectedTotalLen}`);
        }
      }

      const image = new Uint8Array(expectedTotalLen ?? 0);
      let cursor = 0;
      for (const chunk of chunks) {
        image.set(chunk, cursor);
        cursor += chunk.length;
      }

      const parsed = parseConfigFromEeprom(image);
      setConfig((prev) => ({ ...prev, ...parsed.config }));
      log(`CONFIG: loaded ${parsed.records.length} TLV record(s) from connected USB`);
      notify("success", "Imported values from the connected USB device.");
    } catch (err) {
      reportError(err, "Failed to load the connected USB values.");
    }
  }, [log, notify, parseConfigFromEeprom, reportError, transferRaw]);

  const getVersionsNoBusy = useCallback(async () => {
    const resp = await transferRaw(new Uint8Array([CMD_GET_VERSIONS]));
    if (!resp || resp[0] !== CMD_GET_VERSIONS || resp.length < 6) {
      throw new Error("VERSIONS: invalid response");
    }
    if (resp[1] !== 0x00) {
      throw new Error("VERSIONS: device returned error");
    }

    const nextVersions = {
      ch572d: `${resp[2]}.${resp[3]}.${resp[4]}`,
      ch32v203: null
    };
    if (resp.length >= 9) {
      nextVersions.ch32v203 = `${resp[5]}.${resp[6]}.${resp[7]}`;
    }
    return nextVersions;
  }, [transferRaw]);

  const refreshVersions = useCallback(async () => {
    try {
      await withBusy(async () => {
        if (!deviceRef.current) return;
        const nextVersions = await getVersionsNoBusy();
        setVersions(nextVersions);
        const message = `VERSIONS: CH572D ${nextVersions.ch572d} | CH32V203 ${nextVersions.ch32v203 ?? "unavailable"}`;
        log(message);
        notify("success", "Versions refreshed.");
      });
    } catch (err) {
      reportError(err, "Failed to refresh versions.");
    }
  }, [getVersionsNoBusy, log, notify, reportError, withBusy]);

  const connect = useCallback(async () => {
    if (!webUsbSupport.supported) {
      notify("error", webUsbSupport.reason);
      return;
    }

    try {
      await withBusy(async () => {
        let device;
        try {
          log("CONNECT: waiting for device selection");
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

        const productName = device.productName || "USBsp";
        log(`CONNECT: selected ${productName} (${hex4(device.vendorId)}:${hex4(device.productId)})`);

        try {
          log("CONNECT: opening device");
          await withPromiseTimeout(device.open(), USB_CONNECT_STEP_TIMEOUT_MS, "USB open");

          if (device.configuration === null) {
            log("CONNECT: selecting configuration 1");
            await withPromiseTimeout(device.selectConfiguration(1), USB_CONNECT_STEP_TIMEOUT_MS, "USB selectConfiguration");
          }

          const currentConfig =
            device.configuration ??
            device.configurations?.find((entry) => entry.configurationValue === 1) ??
            null;
          const interfaceNumbers = (currentConfig?.interfaces ?? []).map((entry) => entry.interfaceNumber);
          log(`CONNECT: interfaces [${interfaceNumbers.join(", ") || "none"}]`);
          if (!interfaceNumbers.includes(IFACE)) {
            throw new Error(`USB interface ${IFACE} not found on selected configuration`);
          }

          log(`CONNECT: claiming interface ${IFACE}`);
          await withPromiseTimeout(device.claimInterface(IFACE), USB_CONNECT_STEP_TIMEOUT_MS, "USB claimInterface");
        } catch (err) {
          try {
            if (device.opened) {
              await withPromiseTimeout(device.close(), 3000, "USB close");
            }
          } catch {
            // ignore cleanup failures after a connect error
          }
          throw err;
        }

        deviceRef.current = device;
        setConnected(true);
        log(`Connected: ${productName}`);
        notify("success", `Connected to ${productName}.`);

        try {
          const nextVersions = await getVersionsNoBusy();
          setVersions(nextVersions);
          log(`VERSIONS: CH572D ${nextVersions.ch572d} | CH32V203 ${nextVersions.ch32v203 ?? "unavailable"}`);
        } catch (err) {
          log(`VERSIONS: ${err?.message ?? String(err)}`);
        }

        try {
          await readConfigNoBusy();
        } catch (err) {
          log(`CONFIG: auto-read failed: ${err?.message ?? String(err)}`);
        }
      });
    } catch (err) {
      reportError(err, "Failed to connect to the device.");
    }
  }, [getVersionsNoBusy, log, notify, readConfigNoBusy, reportError, webUsbSupport, withBusy]);

  const disconnect = useCallback(async () => {
    try {
      captureLoopRef.current = false;
      flushCapturedPackets(true);
      flushDroppedCaptureLog(true);

      const device = deviceRef.current;
      try {
        if (device?.opened) {
          try {
            await device.releaseInterface(IFACE);
          } catch {
            // ignore
          }
          try {
            await device.close();
          } catch {
            // ignore
          }
        }
      } finally {
        deviceRef.current = null;
        setConnected(false);
        setCaptureRunning(false);
        setVersions({ ch572d: null, ch32v203: null });
        capturePendingPacketsRef.current = [];
        capturePendingDroppedRef.current = 0;
        setCaptureDroppedTotal(0);
        log("Disconnected");
        notify("info", "Disconnected.");
      }
    } catch (err) {
      reportError(err, "Failed to disconnect cleanly.");
    }
  }, [flushCapturedPackets, flushDroppedCaptureLog, log, notify, reportError]);

  const writeConfig = useCallback(async () => {
    try {
      await withBusy(async () => {
        if (!deviceRef.current) return;

        const nextConfig = normalizeConfig(config);
        if (deviceConfigSnapshot) {
          const snapshot = normalizeConfig(deviceConfigSnapshot);
          const same = Object.keys(nextConfig).every((key) => nextConfig[key] === snapshot[key]);
          if (same) {
            log("CONFIG: no changes (skipping write)");
            notify("info", "No changes to save.");
            return;
          }
        }

        const encoder = new TextEncoder();
        const tlvs = [];
        if (nextConfig.vid) tlvs.push(encodeTlv(TLV_VID, u16leBytes(Number.parseInt(nextConfig.vid, 16))));
        if (nextConfig.pid) tlvs.push(encodeTlv(TLV_PID, u16leBytes(Number.parseInt(nextConfig.pid, 16))));
        if (nextConfig.bcdDevice) {
          tlvs.push(encodeTlv(TLV_BCD_DEVICE, u16leBytes(Number.parseInt(nextConfig.bcdDevice, 16))));
        }

        const flags =
          (nextConfig.selfPowered ? FLAG_SELF_POWERED : 0) |
          (nextConfig.remoteWakeup ? FLAG_REMOTE_WAKEUP : 0) |
          (nextConfig.bootConnected ? FLAG_BOOT_CONNECTED : 0);

        tlvs.push(encodeTlv(TLV_MAX_POWER_MA, u16leBytes(nextConfig.maxPowerMa)));
        tlvs.push(encodeTlv(TLV_FLAGS, new Uint8Array([flags & 0xff])));
        tlvs.push(encodeTlv(TLV_ATTACH_DELAY_MS, u16leBytes(nextConfig.attachDelayMs)));
        tlvs.push(encodeTlv(TLV_CAPTURE_MAX_BYTES, u16leBytes(nextConfig.captureMaxBytes)));
        tlvs.push(encodeTlv(TLV_MANUFACTURER, encoder.encode(nextConfig.manufacturer).slice(0, 60)));
        tlvs.push(encodeTlv(TLV_PRODUCT, encoder.encode(nextConfig.product).slice(0, 60)));
        tlvs.push(encodeTlv(TLV_SERIAL, encoder.encode(nextConfig.serial).slice(0, 60)));

        const image = new Uint8Array(EEPROM_SIZE);
        image.fill(0xff);
        const packed = packTlvs(tlvs, EEPROM_SIZE - 1);
        let writeLen = 0;
        for (const part of packed) {
          image.set(part, writeLen);
          writeLen += part.length;
        }
        image[writeLen] = 0x00;

        let writes = 0;
        for (let addr = 0; addr < EEPROM_SIZE; addr += 60) {
          const batch = image.slice(addr, Math.min(addr + 60, EEPROM_SIZE));
          const payload = new Uint8Array(4 + batch.length);
          payload[0] = CMD_EEPROM_WRITE;
          payload[1] = addr & 0xff;
          payload[2] = (addr >> 8) & 0xff;
          payload[3] = batch.length & 0xff;
          payload.set(batch, 4);
          const resp = await transferRaw(payload);
          if (!resp || resp[0] !== CMD_EEPROM_WRITE || resp[1] !== 0x00) {
            const status = resp ? resp[1] : null;
            const error = resp && resp.length >= 4 ? resp[3] : null;
            throw new Error(`CONFIG: write failed (status=${status ?? "N/A"} err=${error ?? "N/A"})`);
          }
          writes += 1;
        }

        const resetResp = await transferRaw(new Uint8Array([CMD_CH32_RESET]), {
          inLen: 16
        });
        if (resetResp && resetResp.length > 0) {
          if (resetResp[0] !== CMD_CH32_RESET) {
            throw new Error("CH32 reset: invalid response");
          }
          if (resetResp.length > 1 && resetResp[1] !== 0x00) {
            throw new Error(`CH32 reset failed (status=${resetResp[1]})`);
          }
        }

        setDeviceConfigSnapshot(nextConfig);
        log(`CONFIG: wrote ${tlvs.length} record(s) in ${writes} write(s)`);
        log("CH32 reset: command sent after save");
        notify("success", "Settings saved.");
      });
    } catch (err) {
      reportError(err, "Failed to write configuration.");
    }
  }, [config, deviceConfigSnapshot, log, notify, reportError, transferRaw, withBusy]);

  const dumpEeprom = useCallback(async () => {
    try {
      await withBusy(async () => {
        if (!deviceRef.current) return;
        const bytes = await readEepromAll();
        const name = `usbsp-eeprom-${new Date().toISOString().replaceAll(":", "").replaceAll("-", "").replace(".", "").replace("Z", "Z")}.bin`;
        downloadBytes(name, bytes);
        notify("success", "Storage dump downloaded.");
      });
    } catch (err) {
      reportError(err, "Failed to dump EEPROM.");
    }
  }, [downloadBytes, notify, readEepromAll, reportError, withBusy]);

  const readEeprom = useCallback(async () => {
    try {
      await withBusy(async () => {
        const addr = clampInt(eepAddr, 0, 255, 0);
        const len = clampInt(eepLen, 1, 61, 1);
        const payload = new Uint8Array([CMD_EEPROM_READ, addr & 0xff, len & 0xff]);
        const data = await transferRaw(payload);
        if (!data || data[0] !== CMD_EEPROM_READ) {
          throw new Error("EEPROM: invalid response");
        }
        const readLen = data[2];
        const body = data.slice(3, 3 + readLen);
        const message = `0x${addr.toString(16).padStart(2, "0")}: ${bytesToHex(body)}`;
        setEepromReadResult(message);
        log(`EEPROM[${addr}] len=${readLen}: ${bytesToHex(body)}`);
        notify("success", "Storage block read.");
      });
    } catch (err) {
      reportError(err, "Failed to read EEPROM.");
    }
  }, [eepAddr, eepLen, log, notify, reportError, transferRaw, withBusy]);

  const sendRaw = useCallback(async () => {
    if (!String(raw ?? "").trim()) {
      notify("warning", "Enter hex bytes.");
      return;
    }
    if (rawError) {
      notify("warning", rawError);
      return;
    }
    try {
      await withBusy(async () => {
        const payload = parseHexInput(raw);
        const data = await transferRaw(payload);
        if (!data) throw new Error("RAW: empty response");
        const message = bytesToHex(data);
        setRawResponse(message);
        log(`RAW RX: ${message}`);
        notify("success", "Raw command sent.");
      });
    } catch (err) {
      reportError(err, "Failed to send the raw command.");
    }
  }, [log, notify, raw, rawError, reportError, transferRaw, withBusy]);

  const spiTest = useCallback(async () => {
    try {
      await withBusy(async () => {
        const data = await transferRaw(new Uint8Array([CMD_SPI_PINGPONG]));
        if (!data || data[0] !== CMD_SPI_PINGPONG) {
          throw new Error("SPI: invalid response");
        }
        const textLength = data[2] ?? 0;
        const responseBytes = trimTrailingZeroes(data.slice(3, 3 + textLength));
        const responseText = new TextDecoder().decode(responseBytes);
        setSpiResponse(responseText || bytesToHex(responseBytes) || "(empty response)");
        log("SPI TX: ping-pong test");
        log(`SPI RX: "${responseText || bytesToHex(responseBytes) || "(empty response)"}"`);
        notify("success", "SPI ping-pong completed.");
      });
    } catch (err) {
      reportError(err, "SPI check failed.");
    }
  }, [log, notify, reportError, transferRaw, withBusy]);

  const resetCh32 = useCallback(async () => {
    try {
      await withBusy(async () => {
        const response = await transferRaw(new Uint8Array([CMD_CH32_RESET]), {
          inLen: 16
        });

        if (response && response.length > 0) {
          if (response[0] !== CMD_CH32_RESET) {
            throw new Error("CH32 reset: invalid response");
          }
          if (response.length > 1 && response[1] !== 0x00) {
            throw new Error(`CH32 reset failed (status=${response[1]})`);
          }
        }

        log("CH32 reset: command sent");
        notify("success", "CH32V203 reset triggered.");
      });
    } catch (err) {
      reportError(err, "Failed to trigger CH32V203 reset.");
    }
  }, [log, notify, reportError, transferRaw, withBusy]);

  const clearPackets = useCallback(() => {
    packetIndexRef.current = 0;
    capturePendingPacketsRef.current = [];
    capturePendingDroppedRef.current = 0;
    captureLastFlushMsRef.current = 0;
    captureLastDropLogMsRef.current = 0;
    setPackets([]);
    setCaptureDroppedTotal(0);
  }, []);

  const drainCaptureBacklog = useCallback(async () => {
    let drainedFrames = 0;
    let emptyFrames = 0;

    for (let attempt = 0; attempt < 24; attempt += 1) {
      const frame = await transferRaw(new Uint8Array([CMD_CAPTURE_POLL]), { inLen: 64 });
      if (!hasCaptureContent(frame)) {
        emptyFrames += 1;
        if (emptyFrames >= 2) break;
        continue;
      }

      drainedFrames += 1;
      emptyFrames = 0;
    }

    if (drainedFrames > 0) {
      log(`CAPTURE: cleared ${drainedFrames} buffered frame(s) before start`);
    }
  }, [log, transferRaw]);

  const saveCapture = useCallback(() => {
    if (!packets.length) {
      notify("info", "No capture data to export.");
      return;
    }

    const content = JSON.stringify(
      {
        savedAt: new Date().toISOString(),
        packets
      },
      null,
      2
    );
    const filename = `usbsp-capture-${new Date().toISOString().replaceAll(":", "").replaceAll("-", "").replace(".", "").replace("Z", "Z")}.json`;
    downloadBytes(filename, content, "application/json");
    notify("success", "Capture exported.");
  }, [downloadBytes, notify, packets]);

  const startCapture = useCallback(async () => {
    if (captureControlBusyRef.current || captureRunning || !deviceRef.current) return;
    captureControlBusyRef.current = true;
    captureStopRequestedRef.current = false;
    packetIndexRef.current = 0;
    capturePendingPacketsRef.current = [];
    capturePendingDroppedRef.current = 0;
    captureLastFlushMsRef.current = 0;
    captureLastDropLogMsRef.current = 0;
    setPackets([]);
    setCaptureDroppedTotal(0);

    try {
      await drainCaptureBacklog();
      packetIndexRef.current = 0;
      capturePendingPacketsRef.current = [];
      capturePendingDroppedRef.current = 0;
      setPackets([]);
      setCaptureDroppedTotal(0);
      captureLoopRef.current = true;
      setCaptureRunning(true);
      log("CAPTURE: started");
      notify("success", "Live capture started.");
      captureControlBusyRef.current = false;

      while (captureLoopRef.current && deviceRef.current) {
        const frame = await transferRaw(new Uint8Array([CMD_CAPTURE_POLL]), { inLen: 64 });
        if (!frame) continue;

        const decoded = decodeCaptureFrame(frame, packetIndexRef);
        if (decoded.dropped) {
          capturePendingDroppedRef.current += decoded.dropped;
          setCaptureDroppedTotal((prev) => prev + decoded.dropped);
          flushDroppedCaptureLog(false);
        }
        if (decoded.packets.length) {
          capturePendingPacketsRef.current.push(...decoded.packets);
          flushCapturedPackets(false);
        } else {
          flushCapturedPackets(false);
          await new Promise((resolve) => setTimeout(resolve, 4));
        }
      }
    } catch (err) {
      reportError(err, "Capture failed.");
    } finally {
      flushCapturedPackets(true);
      flushDroppedCaptureLog(true);
      captureControlBusyRef.current = true;
      captureLoopRef.current = false;
      setCaptureRunning(false);
      captureControlBusyRef.current = false;
      captureStopRequestedRef.current = false;
    }
  }, [captureRunning, drainCaptureBacklog, flushCapturedPackets, flushDroppedCaptureLog, log, notify, reportError, transferRaw]);

  const stopCapture = useCallback(async () => {
    if (!captureRunning || captureControlBusyRef.current || !deviceRef.current) return;
    captureControlBusyRef.current = true;
    captureStopRequestedRef.current = true;
    captureLoopRef.current = false;
    try {
      flushCapturedPackets(true);
      flushDroppedCaptureLog(true);
      log("CAPTURE: stopped");
      notify("info", "Capture stopped.");
    } catch (err) {
      reportError(err, "Failed to stop capture cleanly.");
    } finally {
      captureControlBusyRef.current = false;
    }
  }, [captureRunning, flushCapturedPackets, flushDroppedCaptureLog, log, notify, reportError]);

  useEffect(() => {
    const onDisconnect = (event) => {
      const device = deviceRef.current;
      if (device && event.device === device) {
        disconnect().catch(() => {});
      }
    };

    navigator.usb?.addEventListener?.("disconnect", onDisconnect);
    return () => navigator.usb?.removeEventListener?.("disconnect", onDisconnect);
  }, [disconnect]);

  return {
    busy,
    captureRunning,
    captureDroppedTotal,
    config,
    connected,
    disableActions: busy || !connected || captureRunning,
    disableConnect: connected ? false : busy || !webUsbSupport.supported,
    eepAddr,
    eepLen,
    eepromReadResult,
    packets,
    raw,
    rawError,
    rawResponse,
    spiResponse,
    usbLookupOpen,
    versions,
    webUsbSupported: webUsbSupport.supported,
    webUsbUnavailableReason: webUsbSupport.reason,
    setConfig,
    setEepAddr,
    setEepLen,
    setRaw,
    setUsbLookupOpen,
    clearPackets,
    saveCapture,
    connect,
    disconnect,
    dumpEeprom,
    loadConnectedUsbConfig,
    readConfig,
    readEeprom,
    refreshVersions,
    sendRaw,
    spiTest,
    resetCh32,
    startCapture,
    stopCapture,
    writeConfig
  };
}
