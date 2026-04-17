import React from "react";
import {
  Button,
  Card,
  CardContent,
  CardHeader,
  Divider,
  FormControlLabel,
  IconButton,
  InputAdornment,
  Stack,
  Switch,
  TextField
} from "@mui/material";
import KeyboardArrowUpIcon from "@mui/icons-material/KeyboardArrowUp";
import KeyboardArrowDownIcon from "@mui/icons-material/KeyboardArrowDown";
import MemoryIcon from "@mui/icons-material/Memory";
import DownloadIcon from "@mui/icons-material/Download";
import SendIcon from "@mui/icons-material/Send";
import SearchIcon from "@mui/icons-material/Search";
import UsbIcon from "@mui/icons-material/Usb";

import { clampInt, sanitizeHex4Input } from "../lib/usbsp/utils.js";

function HexStepperField({ label, value, onChange, placeholder }) {
  const parsedValue = Number.parseInt(String(value || "0"), 16);
  const safeValue = Number.isFinite(parsedValue) ? parsedValue & 0xffff : 0;

  const setNumericValue = (next) => {
    onChange({
      target: {
        value: next.toString(16).padStart(4, "0")
      }
    });
  };

  return (
    <TextField
      label={label}
      value={value}
      onChange={onChange}
      size="small"
      inputProps={{ inputMode: "numeric", maxLength: 4 }}
      placeholder={placeholder}
      sx={{ flex: 1 }}
      InputProps={{
        endAdornment: (
          <InputAdornment position="end" sx={{ mr: -0.25 }}>
            <Stack spacing={0} sx={{ mr: -0.15 }}>
              <IconButton size="small" edge="end" sx={{ p: 0.2 }} onClick={() => setNumericValue((safeValue + 1) & 0xffff)}>
                <KeyboardArrowUpIcon fontSize="inherit" />
              </IconButton>
              <IconButton size="small" edge="end" sx={{ p: 0.2 }} onClick={() => setNumericValue((safeValue - 1) & 0xffff)}>
                <KeyboardArrowDownIcon fontSize="inherit" />
              </IconButton>
            </Stack>
          </InputAdornment>
        )
      }}
    />
  );
}

export default function ConfigurationTab({
  config,
  setConfig,
  disableActions,
  onLoadConnectedUsb,
  onReadFromDevice,
  onWriteToDevice,
  onOpenUsbLookup
}) {
  const cardSx = { overflow: "hidden" };

  return (
    <Stack spacing={2}>
      <Card variant="outlined" sx={cardSx}>
        <CardHeader
          avatar={<MemoryIcon />}
          title="USB Settings"
          subheader="VID, PID, strings, and startup options."
          action={
            <Stack direction="row" spacing={0.5}>
              <IconButton size="small" onClick={onLoadConnectedUsb} disabled={disableActions}>
                <UsbIcon fontSize="small" />
              </IconButton>
              <Button variant="outlined" size="small" startIcon={<SearchIcon />} onClick={onOpenUsbLookup}>
                USB IDs
              </Button>
            </Stack>
          }
        />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack spacing={2}>
            <Stack direction={{ xs: "column", sm: "row" }} spacing={1.5}>
              <HexStepperField
                label="VID (hex)"
                value={config.vid}
                onChange={(e) => setConfig((prev) => ({ ...prev, vid: sanitizeHex4Input(e.target.value) }))}
                placeholder="1209"
              />
              <HexStepperField
                label="PID (hex)"
                value={config.pid}
                onChange={(e) => setConfig((prev) => ({ ...prev, pid: sanitizeHex4Input(e.target.value) }))}
                placeholder="CD00"
              />
              <HexStepperField
                label="bcdDevice (hex)"
                value={config.bcdDevice}
                onChange={(e) => setConfig((prev) => ({ ...prev, bcdDevice: sanitizeHex4Input(e.target.value) }))}
                placeholder="0100"
              />
            </Stack>

            <Stack direction={{ xs: "column", sm: "row" }} spacing={1.5}>
              <TextField
                label="Manufacturer"
                value={config.manufacturer}
                onChange={(e) => setConfig((prev) => ({ ...prev, manufacturer: e.target.value }))}
                size="small"
                inputProps={{ maxLength: 60 }}
                helperText="Max 60 bytes"
                sx={{ flex: 1 }}
              />
              <TextField
                label="Product"
                value={config.product}
                onChange={(e) => setConfig((prev) => ({ ...prev, product: e.target.value }))}
                size="small"
                inputProps={{ maxLength: 60 }}
                helperText="Max 60 bytes"
                sx={{ flex: 1 }}
              />
            </Stack>

            <TextField
              label="Serial"
              value={config.serial}
              onChange={(e) => setConfig((prev) => ({ ...prev, serial: e.target.value }))}
              size="small"
              inputProps={{ maxLength: 60 }}
              helperText="Max 60 bytes"
            />

            <Stack direction={{ xs: "column", sm: "row" }} spacing={1.5}>
              <TextField
                label="Max Power (mA)"
                type="number"
                value={config.maxPowerMa}
                onChange={(e) => setConfig((prev) => ({ ...prev, maxPowerMa: clampInt(e.target.value, 0, 500, 100) }))}
                size="small"
                inputProps={{ min: 0, max: 500 }}
                sx={{ flex: 1 }}
              />
              <TextField
                label="Attach Delay (ms)"
                type="number"
                value={config.attachDelayMs}
                onChange={(e) => setConfig((prev) => ({ ...prev, attachDelayMs: clampInt(e.target.value, 0, 60000, 0) }))}
                size="small"
                inputProps={{ min: 0, max: 60000 }}
                sx={{ flex: 1 }}
              />
              <TextField
                label="Capture Max Bytes"
                type="number"
                value={config.captureMaxBytes}
                onChange={(e) => setConfig((prev) => ({ ...prev, captureMaxBytes: clampInt(e.target.value, 0, 512, 64) }))}
                size="small"
                inputProps={{ min: 0, max: 512 }}
                sx={{ flex: 1 }}
              />
            </Stack>

            <Stack direction={{ xs: "column", sm: "row" }} spacing={1.5}>
              <FormControlLabel
                control={<Switch checked={config.selfPowered} onChange={(e) => setConfig((prev) => ({ ...prev, selfPowered: e.target.checked }))} />}
                label="Self powered"
              />
              <FormControlLabel
                control={<Switch checked={config.remoteWakeup} onChange={(e) => setConfig((prev) => ({ ...prev, remoteWakeup: e.target.checked }))} />}
                label="Remote wakeup"
              />
              <FormControlLabel
                control={<Switch checked={config.bootConnected} onChange={(e) => setConfig((prev) => ({ ...prev, bootConnected: e.target.checked }))} />}
                label="Boot connected"
              />
              <FormControlLabel
                control={<Switch checked={config.captureEnabledOnBoot} onChange={(e) => setConfig((prev) => ({ ...prev, captureEnabledOnBoot: e.target.checked }))} />}
                label="Capture on boot"
              />
            </Stack>

            <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap">
              <Button variant="outlined" startIcon={<DownloadIcon />} onClick={onReadFromDevice} disabled={disableActions}>
                Load
              </Button>
              <Button variant="contained" startIcon={<SendIcon />} onClick={onWriteToDevice} disabled={disableActions}>
                Save
              </Button>
            </Stack>
          </Stack>
        </CardContent>
      </Card>
    </Stack>
  );
}
