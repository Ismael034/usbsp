import React from "react";
import {
  Button,
  Card,
  CardContent,
  CardHeader,
  Divider,
  FormControlLabel,
  Stack,
  Switch,
  TextField
} from "@mui/material";
import MemoryIcon from "@mui/icons-material/Memory";
import DownloadIcon from "@mui/icons-material/Download";
import SendIcon from "@mui/icons-material/Send";
import SearchIcon from "@mui/icons-material/Search";

import { clampInt, sanitizeHex4Input } from "../lib/usbsp/utils.js";

export default function ConfigurationTab({
  config,
  setConfig,
  disableActions,
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
          title="EEPROM Parameters"
          subheader="Edit the redirector configuration. Writes are persisted to EEPROM."
          action={
            <Button variant="outlined" size="small" startIcon={<SearchIcon />} onClick={onOpenUsbLookup}>
              Lookup VID/PID
            </Button>
          }
        />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack spacing={2}>
            <Stack direction={{ xs: "column", sm: "row" }} spacing={1.5}>
              <TextField
                label="VID (hex)"
                value={config.vid}
                onChange={(e) => setConfig((p) => ({ ...p, vid: sanitizeHex4Input(e.target.value) }))}
                size="small"
                inputProps={{ inputMode: "numeric", maxLength: 4 }}
                placeholder="1209"
                sx={{ flex: 1 }}
              />
              <TextField
                label="PID (hex)"
                value={config.pid}
                onChange={(e) => setConfig((p) => ({ ...p, pid: sanitizeHex4Input(e.target.value) }))}
                size="small"
                inputProps={{ inputMode: "numeric", maxLength: 4 }}
                placeholder="0001"
                sx={{ flex: 1 }}
              />
              <TextField
                label="bcdDevice (hex)"
                value={config.bcdDevice}
                onChange={(e) => setConfig((p) => ({ ...p, bcdDevice: sanitizeHex4Input(e.target.value) }))}
                size="small"
                inputProps={{ inputMode: "numeric", maxLength: 4 }}
                placeholder="0100"
                sx={{ flex: 1 }}
              />
            </Stack>

            <Stack direction={{ xs: "column", sm: "row" }} spacing={1.5}>
              <TextField
                label="Manufacturer"
                value={config.manufacturer}
                onChange={(e) => setConfig((p) => ({ ...p, manufacturer: e.target.value }))}
                size="small"
                inputProps={{ maxLength: 60 }}
                helperText="Max 60 bytes"
                sx={{ flex: 1 }}
              />
              <TextField
                label="Product"
                value={config.product}
                onChange={(e) => setConfig((p) => ({ ...p, product: e.target.value }))}
                size="small"
                inputProps={{ maxLength: 60 }}
                helperText="Max 60 bytes"
                sx={{ flex: 1 }}
              />
            </Stack>

            <TextField
              label="Serial"
              value={config.serial}
              onChange={(e) => setConfig((p) => ({ ...p, serial: e.target.value }))}
              size="small"
              inputProps={{ maxLength: 60 }}
              helperText="Max 60 bytes"
            />

            <Stack direction={{ xs: "column", sm: "row" }} spacing={1.5}>
              <TextField
                label="Max Power (mA)"
                type="number"
                value={config.maxPowerMa}
                onChange={(e) => setConfig((p) => ({ ...p, maxPowerMa: clampInt(e.target.value, 0, 500, 100) }))}
                size="small"
                inputProps={{ min: 0, max: 500 }}
                sx={{ flex: 1 }}
              />
              <TextField
                label="Attach Delay (ms)"
                type="number"
                value={config.attachDelayMs}
                onChange={(e) =>
                  setConfig((p) => ({ ...p, attachDelayMs: clampInt(e.target.value, 0, 60000, 0) }))
                }
                size="small"
                inputProps={{ min: 0, max: 60000 }}
                sx={{ flex: 1 }}
              />
              <TextField
                label="Capture Max Bytes"
                type="number"
                value={config.captureMaxBytes}
                onChange={(e) =>
                  setConfig((p) => ({ ...p, captureMaxBytes: clampInt(e.target.value, 0, 512, 64) }))
                }
                size="small"
                inputProps={{ min: 0, max: 512 }}
                sx={{ flex: 1 }}
              />
            </Stack>

            <Stack direction={{ xs: "column", sm: "row" }} spacing={1.5}>
              <FormControlLabel
                control={<Switch checked={config.selfPowered} onChange={(e) => setConfig((p) => ({ ...p, selfPowered: e.target.checked }))} />}
                label="Self powered"
              />
              <FormControlLabel
                control={<Switch checked={config.remoteWakeup} onChange={(e) => setConfig((p) => ({ ...p, remoteWakeup: e.target.checked }))} />}
                label="Remote wakeup"
              />
              <FormControlLabel
                control={<Switch checked={config.bootConnected} onChange={(e) => setConfig((p) => ({ ...p, bootConnected: e.target.checked }))} />}
                label="Boot connected"
              />
              <FormControlLabel
                control={<Switch checked={config.captureEnabledOnBoot} onChange={(e) => setConfig((p) => ({ ...p, captureEnabledOnBoot: e.target.checked }))} />}
                label="Capture on boot"
              />
            </Stack>

            <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap">
              <Button variant="outlined" startIcon={<DownloadIcon />} onClick={onReadFromDevice} disabled={disableActions}>
                Read From Device
              </Button>
              <Button variant="contained" startIcon={<SendIcon />} onClick={onWriteToDevice} disabled={disableActions}>
                Write To Device
              </Button>
            </Stack>
          </Stack>
        </CardContent>
      </Card>
    </Stack>
  );
}
