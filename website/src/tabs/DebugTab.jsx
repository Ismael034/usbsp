import React from "react";
import {
  Button,
  Card,
  CardContent,
  CardHeader,
  Chip,
  Divider,
  Stack,
  TextField,
  Typography
} from "@mui/material";
import MemoryIcon from "@mui/icons-material/Memory";
import DownloadIcon from "@mui/icons-material/Download";
import InfoOutlinedIcon from "@mui/icons-material/InfoOutlined";
import TerminalIcon from "@mui/icons-material/Terminal";
import SendIcon from "@mui/icons-material/Send";
import ScienceIcon from "@mui/icons-material/Science";
import RestartAltIcon from "@mui/icons-material/RestartAlt";

import { clampInt } from "../lib/usbsp/utils.js";

function ResultBox({ label, value }) {
  if (!value) return null;
  return (
    <Stack spacing={0.5}>
      <Typography variant="caption" sx={{ color: "text.secondary", fontWeight: 700 }}>
        {label}
      </Typography>
      <Typography
        component="pre"
        sx={{
          m: 0,
          p: 1.5,
          borderRadius: 2,
          border: "1px solid",
          borderColor: "divider",
          bgcolor: "background.paper",
          fontFamily: '"JetBrains Mono","Fira Code",monospace',
          fontSize: 12,
          whiteSpace: "pre-wrap",
          overflowWrap: "anywhere"
        }}
      >
        {value}
      </Typography>
    </Stack>
  );
}

export default function DebugTab({
  eepAddr,
  setEepAddr,
  eepLen,
  setEepLen,
  raw,
  setRaw,
  rawError,
  versions,
  webVersion,
  disableActions,
  onDumpEeprom,
  onReadEeprom,
  onRefreshVersions,
  onSendRaw,
  onSpiTest,
  onResetCh32,
  eepromReadResult,
  rawResponse,
  spiResponse
}) {
  const cardSx = { overflow: "hidden" };
  const ch572dFwAvailable = !!versions?.ch572d;
  const ch32v203FwAvailable = !!versions?.ch32v203;

  return (
    <Stack spacing={2}>
      <Card variant="outlined" sx={cardSx}>
        <CardHeader avatar={<MemoryIcon />} title="Storage" subheader="Dump or read stored bytes" />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack spacing={2}>
            <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap" alignItems="center">
              <Button variant="outlined" startIcon={<DownloadIcon />} onClick={onDumpEeprom} disabled={disableActions}>
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
            <ResultBox label="Read result" value={eepromReadResult} />
          </Stack>
        </CardContent>
      </Card>

      <Card variant="outlined" sx={cardSx}>
        <CardHeader
          avatar={<InfoOutlinedIcon />}
          title="Versions"
          subheader="Read device versions"
          action={
            <Stack direction="row" spacing={1}>
              <Button variant="outlined" size="small" onClick={onResetCh32} disabled={disableActions}>
                CH32 Reset
              </Button>
              <Button variant="outlined" size="small" onClick={onRefreshVersions} disabled={disableActions}>
                Refresh
              </Button>
            </Stack>
          }
        />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap">
            <Chip label={`Web: ${webVersion}`} variant="outlined" />
            <Chip label={`CH572D FW: ${versions?.ch572d ?? "N/A"}`} variant="outlined" sx={ch572dFwAvailable ? null : { opacity: 0.55 }} />
            <Chip
              label={`CH32V203 FW: ${versions?.ch32v203 ?? "N/A"}`}
              variant="outlined"
              sx={ch32v203FwAvailable ? null : { opacity: 0.55 }}
            />
          </Stack>
        </CardContent>
      </Card>

      <Card variant="outlined" sx={cardSx}>
        <CardHeader avatar={<TerminalIcon />} title="Raw Command" subheader="Send WebUSB commands manually" />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack spacing={2}>
            <Stack direction="row" spacing={1.5} useFlexGap flexWrap="wrap" alignItems="flex-start">
              <TextField
                label="Hex bytes"
                placeholder="010004 or 0x010004"
                value={raw}
                onChange={(e) => setRaw(e.target.value)}
                error={!!rawError}
                helperText={rawError || ""}
                size="small"
                sx={{ minWidth: 240, flex: "1 1 260px" }}
              />
              <Button variant="contained" startIcon={<SendIcon />} onClick={onSendRaw} disabled={disableActions || !!rawError}>
                Send
              </Button>
            </Stack>
            <ResultBox label="Raw response" value={rawResponse} />
          </Stack>
        </CardContent>
      </Card>

      <Card variant="outlined" sx={cardSx}>
        <CardHeader avatar={<ScienceIcon />} title="SPI Test" subheader="Run the SPI link test" />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack spacing={2} alignItems="flex-start">
            <Button variant="contained" startIcon={<ScienceIcon />} onClick={onSpiTest} disabled={disableActions}>
              Run Test
            </Button>
            <ResultBox label="SPI response" value={spiResponse} />
          </Stack>
        </CardContent>
      </Card>

    </Stack>
  );
}
