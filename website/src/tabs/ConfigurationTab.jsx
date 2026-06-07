import React from "react";
import {
  Button,
  Card,
  CardContent,
  CardHeader,
  Divider,
  FormControlLabel,
  IconButton,
  Stack,
  Switch,
  TextField
} from "@mui/material";
import MemoryIcon from "@mui/icons-material/Memory";
import FileDownloadIcon from "@mui/icons-material/FileDownload";
import SaveIcon from "@mui/icons-material/Save";
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

  const handleWheel = (event) => {
    const active = event.currentTarget.querySelector("input") === document.activeElement;
    if (!active) return;
    event.preventDefault();
    setNumericValue((safeValue + (event.deltaY < 0 ? 1 : -1)) & 0xffff);
  };

  const spinnerButtonSx = {
    width: 16,
    height: 11,
    minWidth: 16,
    borderRadius: 0,
    color: "text.secondary",
    bgcolor: "transparent",
    p: 0,
    opacity: 0.72,
    "&:hover": {
      bgcolor: "transparent",
      color: "text.primary"
    }
  };

  const spinnerArrowSx = (direction) => ({
    "&::before": {
      content: '""',
      display: "block",
      width: 0,
      height: 0,
      borderLeft: "3.5px solid transparent",
      borderRight: "3.5px solid transparent",
      ...(direction === "up"
        ? { borderBottom: "4.5px solid currentColor" }
        : { borderTop: "4.5px solid currentColor" })
    }
  });

  return (
    <TextField
      label={label}
      value={value}
      onChange={onChange}
      onWheel={handleWheel}
      size="small"
      inputProps={{ inputMode: "text", maxLength: 4 }}
      placeholder={placeholder}
      sx={{
        flex: 1,
        "& .hex-spinner": {
          opacity: 0,
          pointerEvents: "none"
        },
        "&:hover .hex-spinner, &:focus-within .hex-spinner": {
          opacity: 1,
          pointerEvents: "auto"
        }
      }}
      InputProps={{
        endAdornment: (
          <Stack
            className="hex-spinner"
            spacing={0}
            sx={{
              position: "absolute",
              top: "50%",
              right: 8,
              transform: "translateY(-50%)",
              justifyContent: "center",
              transition: "opacity 120ms ease"
            }}
          >
            <IconButton
              aria-label={`Increment ${label}`}
              size="small"
              tabIndex={-1}
              sx={{ ...spinnerButtonSx, ...spinnerArrowSx("up") }}
              onClick={() => setNumericValue((safeValue + 1) & 0xffff)}
            />
            <IconButton
              aria-label={`Decrement ${label}`}
              size="small"
              tabIndex={-1}
              sx={{ ...spinnerButtonSx, ...spinnerArrowSx("down") }}
              onClick={() => setNumericValue((safeValue - 1) & 0xffff)}
            />
          </Stack>
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
            </Stack>

            <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap">
              <Button variant="outlined" startIcon={<FileDownloadIcon />} onClick={onReadFromDevice} disabled={disableActions}>
                Load
              </Button>
              <Button variant="contained" startIcon={<SaveIcon />} onClick={onWriteToDevice} disabled={disableActions}>
                Save
              </Button>
            </Stack>
          </Stack>
        </CardContent>
      </Card>
    </Stack>
  );
}
