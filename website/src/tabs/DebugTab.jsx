import React from "react";
import { Button, Card, CardContent, CardHeader, Chip, Divider, Stack, TextField } from "@mui/material";
import MemoryIcon from "@mui/icons-material/Memory";
import DownloadIcon from "@mui/icons-material/Download";
import InfoOutlinedIcon from "@mui/icons-material/InfoOutlined";
import TerminalIcon from "@mui/icons-material/Terminal";
import SendIcon from "@mui/icons-material/Send";
import ScienceIcon from "@mui/icons-material/Science";

import { clampInt } from "../lib/usbsp/utils.js";

export default function DebugTab({
  eepAddr,
  setEepAddr,
  eepLen,
  setEepLen,
  raw,
  setRaw,
  versions,
  webVersion,
  disableActions,
  busy,
  connected,
  captureRunning,
  onDumpEeprom,
  onReadEeprom,
  onRefreshVersions,
  onSendRaw,
  onSpiTest
}) {
  const cardSx = { overflow: "hidden" };
  const ch572dFwAvailable = !!versions?.ch572d;
  const ch572dFwLabel = `CH572D FW: ${versions?.ch572d ?? "N/A"}`;
  const ch32v203FwAvailable = !!versions?.ch32v203;
  const ch32v203FwLabel = `CH32V203 FW: ${versions?.ch32v203 ?? "N/A"}`;

  return (
    <Stack spacing={2}>
      <Card variant="outlined" sx={cardSx}>
        <CardHeader avatar={<MemoryIcon />} title="EEPROM" subheader="Dump the full EEPROM or read a range of bytes" />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap" alignItems="center">
            <Button
              variant="outlined"
              startIcon={<DownloadIcon />}
              onClick={onDumpEeprom}
              disabled={disableActions}
            >
              Dump (256B)
            </Button>
            <TextField
              label="Address"
              type="number"
              value={eepAddr}
              onChange={(e) => setEepAddr(clampInt(e.target.value, 0, 255, 0))}
              inputProps={{ min: 0, max: 255 }}
              size="small"
            />
            <TextField
              label="Length"
              type="number"
              value={eepLen}
              onChange={(e) => setEepLen(clampInt(e.target.value, 1, 61, 1))}
              inputProps={{ min: 1, max: 61 }}
              size="small"
            />
            <Button variant="contained" startIcon={<DownloadIcon />} onClick={onReadEeprom} disabled={disableActions}>
              Read
            </Button>
          </Stack>
        </CardContent>
      </Card>

      <Card variant="outlined" sx={cardSx}>
        <CardHeader
          avatar={<InfoOutlinedIcon />}
          title="Versions"
          subheader="Query firmware versions from the connected device"
          action={
            <Button variant="outlined" size="small" onClick={onRefreshVersions} disabled={disableActions}>
              Get Version
            </Button>
          }
        />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap">
            <Chip label={`Web: ${webVersion}`} variant="outlined" />
            <Chip
              label={ch572dFwLabel}
              variant="outlined"
              sx={ch572dFwAvailable ? null : { opacity: 0.55 }}
            />
            <Chip
              label={ch32v203FwLabel}
              variant="outlined"
              sx={ch32v203FwAvailable ? null : { opacity: 0.55 }}
            />
          </Stack>
        </CardContent>
      </Card>

      <Card variant="outlined" sx={cardSx}>
        <CardHeader avatar={<TerminalIcon />} title="Raw Command" subheader="Send raw bytes over WebUSB (debug)" />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack direction="row" spacing={1.5} useFlexGap flexWrap="wrap" alignItems="center">
            <TextField
              label="Hex bytes"
              placeholder="01 00 04"
              value={raw}
              onChange={(e) => setRaw(e.target.value)}
              size="small"
              sx={{ minWidth: 240, flex: "1 1 260px" }}
            />
            <Button variant="contained" startIcon={<SendIcon />} onClick={onSendRaw} disabled={disableActions}>
              Send
            </Button>
          </Stack>
        </CardContent>
      </Card>

      <Card variant="outlined" sx={cardSx}>
        <CardHeader
          avatar={<ScienceIcon />}
          title="SPI Test"
          subheader="Run a ping-pong SPI test using a fixed payload (debug)"
        />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Button variant="contained" startIcon={<ScienceIcon />} onClick={onSpiTest} disabled={disableActions}>
            Run Test
          </Button>
        </CardContent>
      </Card>
    </Stack>
  );
}
