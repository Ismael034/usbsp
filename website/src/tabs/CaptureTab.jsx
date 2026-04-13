import React, { useEffect, useMemo, useState } from "react";
import {
  Box,
  Button,
  Card,
  CardContent,
  CardHeader,
  Divider,
  Dialog,
  DialogContent,
  DialogTitle,
  FormControl,
  IconButton,
  InputLabel,
  MenuItem,
  Select,
  Stack,
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  Tooltip,
  Typography
} from "@mui/material";
import PlayArrowIcon from "@mui/icons-material/PlayArrow";
import StopIcon from "@mui/icons-material/Stop";
import SaveAltIcon from "@mui/icons-material/SaveAlt";
import DeleteSweepIcon from "@mui/icons-material/DeleteSweep";
import ShowChartIcon from "@mui/icons-material/ShowChart";
import { decodeHidMouse, parseHidMouseReport } from "../lib/hidMouse.js";

function parseHexBytes(hexText) {
  return String(hexText ?? "")
    .trim()
    .split(/\s+/)
    .filter(Boolean)
    .map((part) => Number.parseInt(part, 16))
    .filter((value) => Number.isFinite(value) && value >= 0 && value <= 255);
}

function decodeKeyboard(bytes) {
  if (bytes.length !== 8) return "Not a boot-keyboard report";

  const modifierMap = [
    "Left Ctrl",
    "Left Shift",
    "Left Alt",
    "Left Meta",
    "Right Ctrl",
    "Right Shift",
    "Right Alt",
    "Right Meta"
  ];
  const keyMap = {
    0x04: "A",
    0x05: "B",
    0x06: "C",
    0x07: "D",
    0x08: "E",
    0x09: "F",
    0x0a: "G",
    0x0b: "H",
    0x0c: "I",
    0x0d: "J",
    0x0e: "K",
    0x0f: "L",
    0x10: "M",
    0x11: "N",
    0x12: "O",
    0x13: "P",
    0x14: "Q",
    0x15: "R",
    0x16: "S",
    0x17: "T",
    0x18: "U",
    0x19: "V",
    0x1a: "W",
    0x1b: "X",
    0x1c: "Y",
    0x1d: "Z",
    0x1e: "1",
    0x1f: "2",
    0x20: "3",
    0x21: "4",
    0x22: "5",
    0x23: "6",
    0x24: "7",
    0x25: "8",
    0x26: "9",
    0x27: "0",
    0x28: "Enter",
    0x29: "Escape",
    0x2a: "Backspace",
    0x2b: "Tab",
    0x2c: "Space"
  };

  const pressed = [];
  for (let bit = 0; bit < 8; bit += 1) {
    if (bytes[0] & (1 << bit)) pressed.push(modifierMap[bit]);
  }
  for (const code of bytes.slice(2, 8)) {
    if (code === 0) continue;
    pressed.push(keyMap[code] ?? `0x${code.toString(16).padStart(2, "0")}`);
  }
  return pressed.length ? `Keyboard: ${pressed.join(", ")}` : "-";
}

function decodeMouse(bytes) {
  return decodeHidMouse(bytes);
}

function decodeMouse16(bytes) {
  return decodeHidMouse(bytes, { forceWideAxes: true });
}

function decodePacket(packet, decoderMode) {
  const bytes = Array.isArray(packet.data) ? packet.data : parseHexBytes(packet.hex);

  if (decoderMode === "none") {
    return bytes.length ? "Raw USB packet" : "-";
  }
  if (decoderMode === "hid-keyboard") {
    return decodeKeyboard(bytes);
  }
  if (decoderMode === "hid-mouse") {
    return decodeMouse(bytes);
  }
  if (decoderMode === "hid-mouse-16") {
    return decodeMouse16(bytes);
  }

  if (bytes.length === 8) {
    return decodeKeyboard(bytes);
  }
  if (bytes.length >= 1 && bytes.length <= 8 && packet.direction === "IN" && packet.endpoint !== 0) {
    return decodeMouse(bytes);
  }
  return packet.summary || "Raw USB packet";
}

function packetBytes(packet) {
  return Array.isArray(packet.data) ? packet.data : parseHexBytes(packet.hex);
}

function isMousePacketCandidate(packet, bytes, decoderMode) {
  if (!bytes.length) return false;
  if (packet.direction !== "IN" || packet.endpoint === 0) return false;

  if (decoderMode === "hid-keyboard" || decoderMode === "none") {
    return false;
  }

  if (decoderMode === "hid-mouse" || decoderMode === "hid-mouse-16") {
    return bytes.length >= 1 && bytes.length <= 8;
  }

  return bytes.length >= 1 && bytes.length <= 8 && bytes.length !== 8;
}

function buildMouseTrace(packets, decoderMode) {
  let x = 0;
  let y = 0;
  let totalDx = 0;
  let totalDy = 0;
  let movementPackets = 0;
  let mousePackets = 0;
  let wheelTotal = 0;
  let panTotal = 0;
  const points = [{ x: 0, y: 0 }];

  for (const packet of packets.slice(-400)) {
    const bytes = packetBytes(packet);
    if (!isMousePacketCandidate(packet, bytes, decoderMode)) continue;

    const parsed = parseHidMouseReport(bytes, { forceWideAxes: decoderMode === "hid-mouse-16" });
    if (!parsed) continue;

    mousePackets += 1;
    wheelTotal += parsed.wheel;
    panTotal += parsed.pan;

    if (parsed.dx === 0 && parsed.dy === 0) {
      continue;
    }

    movementPackets += 1;
    x += parsed.dx;
    y += parsed.dy;
    totalDx += parsed.dx;
    totalDy += parsed.dy;
    points.push({ x, y });
  }

  if (mousePackets === 0) {
    return null;
  }

  let minX = points[0].x;
  let maxX = points[0].x;
  let minY = points[0].y;
  let maxY = points[0].y;

  for (const point of points) {
    if (point.x < minX) minX = point.x;
    if (point.x > maxX) maxX = point.x;
    if (point.y < minY) minY = point.y;
    if (point.y > maxY) maxY = point.y;
  }

  return {
    points,
    mousePackets,
    movementPackets,
    totalDx,
    totalDy,
    wheelTotal,
    panTotal,
    minX,
    maxX,
    minY,
    maxY,
    currentX: x,
    currentY: y
  };
}

function buildTraceGeometry(trace, width = 640, height = 260, padding = 18) {
  if (!trace) return null;

  const rangeX = Math.max(trace.maxX - trace.minX, 1);
  const rangeY = Math.max(trace.maxY - trace.minY, 1);
  const innerWidth = width - padding * 2;
  const innerHeight = height - padding * 2;
  const scale = Math.min(innerWidth / rangeX, innerHeight / rangeY);
  const offsetX = padding + (innerWidth - rangeX * scale) / 2;
  const offsetY = padding + (innerHeight - rangeY * scale) / 2;

  const mapPoint = (point) => ({
    x: offsetX + (point.x - trace.minX) * scale,
    y: offsetY + (point.y - trace.minY) * scale
  });

  const mappedPoints = trace.points.map(mapPoint);
  const polyline = mappedPoints.map((point) => `${point.x.toFixed(2)},${point.y.toFixed(2)}`).join(" ");
  const current = mappedPoints[mappedPoints.length - 1];
  const originVisible =
    trace.minX <= 0 && trace.maxX >= 0 && trace.minY <= 0 && trace.maxY >= 0 ? mapPoint({ x: 0, y: 0 }) : null;

  return {
    width,
    height,
    polyline,
    current,
    originVisible
  };
}

function drawMouseTraceCanvas(ctx, geometry, scale = 1) {
  const width = geometry.width * scale;
  const height = geometry.height * scale;

  ctx.save();
  ctx.scale(scale, scale);

  ctx.fillStyle = "#f7fafc";
  ctx.fillRect(0, 0, geometry.width, geometry.height);

  ctx.strokeStyle = "#d7dee7";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, geometry.height / 2);
  ctx.lineTo(geometry.width, geometry.height / 2);
  ctx.moveTo(geometry.width / 2, 0);
  ctx.lineTo(geometry.width / 2, geometry.height);
  ctx.stroke();

  if (geometry.originVisible) {
    ctx.setLineDash([4, 4]);
    ctx.strokeStyle = "#90a4ae";
    ctx.beginPath();
    ctx.moveTo(0, geometry.originVisible.y);
    ctx.lineTo(geometry.width, geometry.originVisible.y);
    ctx.moveTo(geometry.originVisible.x, 0);
    ctx.lineTo(geometry.originVisible.x, geometry.height);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  const points = geometry.polyline
    .split(" ")
    .filter(Boolean)
    .map((pair) => pair.split(",").map((value) => Number.parseFloat(value)));

  if (points.length) {
    ctx.strokeStyle = "#0f766e";
    ctx.lineWidth = 2.5;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";
    ctx.beginPath();
    ctx.moveTo(points[0][0], points[0][1]);
    for (let index = 1; index < points.length; index += 1) {
      ctx.lineTo(points[index][0], points[index][1]);
    }
    ctx.stroke();
  }

  ctx.fillStyle = "#dc2626";
  ctx.beginPath();
  ctx.arc(geometry.current.x, geometry.current.y, 5, 0, Math.PI * 2);
  ctx.fill();

  ctx.restore();

  return { width, height };
}

function saveMouseTracePng(geometry) {
  if (!geometry || typeof document === "undefined") return;

  const scale = 2;
  const canvas = document.createElement("canvas");
  canvas.width = geometry.width * scale;
  canvas.height = geometry.height * scale;

  const ctx = canvas.getContext("2d");
  if (!ctx) return;

  drawMouseTraceCanvas(ctx, geometry, scale);

  const link = document.createElement("a");
  link.href = canvas.toDataURL("image/png");
  link.download = "mouse-trace.png";
  link.click();
}

function MouseTraceGraphic({ geometry, height = { xs: 220, md: 260 } }) {
  return (
    <Box
      sx={{
        borderRadius: 1.5,
        overflow: "hidden",
        border: "1px solid",
        borderColor: "divider",
        bgcolor: "background.paper"
      }}
    >
      <Box component="svg" viewBox={`0 0 ${geometry.width} ${geometry.height}`} sx={{ display: "block", width: "100%", height }}>
        <rect width={geometry.width} height={geometry.height} fill="#f7fafc" />
        <path d={`M 0 ${geometry.height / 2} H ${geometry.width}`} stroke="#d7dee7" />
        <path d={`M ${geometry.width / 2} 0 V ${geometry.height}`} stroke="#d7dee7" />
        {geometry.originVisible ? (
          <>
            <path d={`M 0 ${geometry.originVisible.y} H ${geometry.width}`} stroke="#90a4ae" strokeDasharray="4 4" />
            <path d={`M ${geometry.originVisible.x} 0 V ${geometry.height}`} stroke="#90a4ae" strokeDasharray="4 4" />
          </>
        ) : null}
        <polyline
          fill="none"
          points={geometry.polyline}
          stroke="#0f766e"
          strokeWidth="2.5"
          strokeLinejoin="round"
          strokeLinecap="round"
        />
        <circle cx={geometry.current.x} cy={geometry.current.y} r="5" fill="#dc2626" />
      </Box>
    </Box>
  );
}

export default function CaptureTab({
  captureRunning,
  packets,
  disableActions,
  busy,
  connected,
  onStart,
  onStop,
  onClearPackets,
  onSaveCapture
}) {
  const cardSx = { overflow: "hidden" };
  const [decoderMode, setDecoderMode] = useState("auto");
  const [mouseTraceOpen, setMouseTraceOpen] = useState(false);

  const renderedPackets = useMemo(
    () =>
      [...packets]
        .slice(-200)
        .reverse()
        .map((packet) => ({
        ...packet,
        decoded: decodePacket(packet, decoderMode)
      })),
    [decoderMode, packets]
  );

  const mouseTrace = useMemo(() => buildMouseTrace(packets, decoderMode), [decoderMode, packets]);
  const mouseTraceGeometry = useMemo(() => buildTraceGeometry(mouseTrace), [mouseTrace]);

  useEffect(() => {
    if (captureRunning || !mouseTrace) {
      setMouseTraceOpen(false);
    }
  }, [captureRunning, mouseTrace]);

  return (
    <Stack spacing={2}>
      <Card variant="outlined" sx={cardSx}>
        <CardHeader
          title="Live Capture"
          subheader="Live USB traffic"
          action={
            <Stack direction="row" spacing={0.5}>
              <Tooltip title="Export capture">
                <span>
                  <IconButton size="small" onClick={onSaveCapture} disabled={!packets.length}>
                    <SaveAltIcon fontSize="small" />
                  </IconButton>
                </span>
              </Tooltip>
              <Tooltip title="Clear">
                <span>
                  <IconButton size="small" onClick={onClearPackets} disabled={!packets.length}>
                    <DeleteSweepIcon fontSize="small" />
                  </IconButton>
                </span>
              </Tooltip>
            </Stack>
          }
        />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack spacing={2}>
            <Stack direction={{ xs: "column", md: "row" }} spacing={1.5} justifyContent="space-between" useFlexGap>
              <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap">
                <Button
                  variant={captureRunning ? "outlined" : "contained"}
                  startIcon={captureRunning ? <StopIcon /> : <PlayArrowIcon />}
                  onClick={captureRunning ? onStop : onStart}
                  disabled={captureRunning ? busy || !connected : disableActions || captureRunning}
                >
                  {captureRunning ? "Stop" : "Start"}
                </Button>
              </Stack>

              <Stack direction="row" spacing={1.5} alignItems="center" useFlexGap flexWrap="wrap">
                <Typography
                  variant="body2"
                  sx={{
                    color: "text.secondary",
                    display: "inline-flex",
                    alignItems: "center",
                    gap: 0.75
                  }}
                >
                  <Box
                    component="span"
                    sx={{
                      width: 8,
                      height: 8,
                      borderRadius: "50%",
                      bgcolor: captureRunning ? "success.main" : "text.disabled",
                      flex: "0 0 auto"
                    }}
                  />
                  {captureRunning ? "Running" : "Stopped"}
                </Typography>

                <Typography variant="body2" sx={{ color: "text.secondary" }}>
                  {packets.length} packet{packets.length === 1 ? "" : "s"}
                </Typography>

                {mouseTrace && !captureRunning ? (
                  <Tooltip title="Open mouse trace">
                    <span>
                      <IconButton size="small" onClick={() => setMouseTraceOpen(true)}>
                        <ShowChartIcon fontSize="small" />
                      </IconButton>
                    </span>
                  </Tooltip>
                ) : null}

                <FormControl size="small" sx={{ minWidth: 180 }}>
                  <InputLabel id="capture-decoder-label">Decoder</InputLabel>
                  <Select
                    labelId="capture-decoder-label"
                    value={decoderMode}
                    label="Decoder"
                    onChange={(event) => setDecoderMode(event.target.value)}
                  >
                    <MenuItem value="auto">Auto</MenuItem>
                    <MenuItem value="none">None</MenuItem>
                    <MenuItem value="hid-keyboard">HID Keyboard</MenuItem>
                    <MenuItem value="hid-mouse">HID Mouse</MenuItem>
                    <MenuItem value="hid-mouse-16">HID Mouse (16-bit XY)</MenuItem>
                  </Select>
                </FormControl>
              </Stack>
            </Stack>

            <Box
              sx={{
                border: "1px solid",
                borderColor: "divider",
                borderRadius: 2,
                overflow: "hidden"
              }}
            >
              <Box
                sx={{
                  width: "100%",
                  maxHeight: { xs: 420, md: 560 },
                  overflowY: "auto"
                }}
              >
                <Table size="small" sx={{ width: "100%", tableLayout: "fixed" }}>
                  <TableHead>
                    <TableRow>
                      <TableCell sx={{ width: 68, whiteSpace: "nowrap" }}>Seq</TableCell>
                      <TableCell sx={{ width: 64, whiteSpace: "nowrap" }}>Dir</TableCell>
                      <TableCell sx={{ width: 56, whiteSpace: "nowrap" }}>EP</TableCell>
                      <TableCell sx={{ width: 64, whiteSpace: "nowrap" }}>Len</TableCell>
                      <TableCell sx={{ width: "32%" }}>Decoded</TableCell>
                      <TableCell>Data</TableCell>
                    </TableRow>
                  </TableHead>
                  <TableBody>
                    {packets.length === 0 ? (
                      <TableRow>
                        <TableCell colSpan={6}>
                          <Typography variant="body2" sx={{ color: "text.secondary" }}>
                            No packets captured yet.
                          </Typography>
                        </TableCell>
                      </TableRow>
                    ) : (
                      renderedPackets.map((packet) => (
                        <TableRow key={packet.id}>
                          <TableCell sx={{ whiteSpace: "nowrap", verticalAlign: "top" }}>{packet.seq}</TableCell>
                          <TableCell sx={{ whiteSpace: "nowrap", verticalAlign: "top" }}>{packet.direction}</TableCell>
                          <TableCell sx={{ whiteSpace: "nowrap", verticalAlign: "top" }}>{packet.endpoint}</TableCell>
                          <TableCell sx={{ whiteSpace: "nowrap", verticalAlign: "top" }}>{packet.length}</TableCell>
                          <TableCell sx={{ verticalAlign: "top", overflowWrap: "anywhere" }}>{packet.decoded}</TableCell>
                          <TableCell
                            sx={{
                              fontFamily: '"JetBrains Mono","Fira Code",monospace',
                              fontSize: 12,
                              verticalAlign: "top",
                              whiteSpace: "normal",
                              overflowWrap: "anywhere"
                            }}
                          >
                            {packet.hex}
                          </TableCell>
                        </TableRow>
                      ))
                    )}
                  </TableBody>
                </Table>
              </Box>
            </Box>
          </Stack>
        </CardContent>
      </Card>

      <Dialog open={mouseTraceOpen} onClose={() => setMouseTraceOpen(false)} maxWidth="lg" fullWidth>
        <DialogTitle
          sx={{
            display: "flex",
            alignItems: "center",
            justifyContent: "space-between",
            gap: 1
          }}
        >
          <Box component="span">Mouse Trace</Box>
          {mouseTraceGeometry ? (
            <Tooltip title="Save as PNG">
              <span>
                <IconButton size="small" onClick={() => saveMouseTracePng(mouseTraceGeometry)}>
                  <SaveAltIcon fontSize="small" />
                </IconButton>
              </span>
            </Tooltip>
          ) : null}
        </DialogTitle>
        <DialogContent dividers>
          {mouseTrace && mouseTraceGeometry ? (
            <Stack spacing={1.5}>
              <MouseTraceGraphic geometry={mouseTraceGeometry} height={{ xs: 320, md: 520 }} />
              <Stack direction="row" spacing={1.5} useFlexGap flexWrap="wrap">
                <Typography variant="body2" sx={{ color: "text.secondary" }}>
                  Packets {mouseTrace.mousePackets}
                </Typography>
                <Typography variant="body2" sx={{ color: "text.secondary" }}>
                  Moves {mouseTrace.movementPackets}
                </Typography>
                <Typography variant="body2" sx={{ color: "text.secondary" }}>
                  Current {mouseTrace.currentX}, {mouseTrace.currentY}
                </Typography>
                <Typography variant="body2" sx={{ color: "text.secondary" }}>
                  Sum dx {mouseTrace.totalDx}
                </Typography>
                <Typography variant="body2" sx={{ color: "text.secondary" }}>
                  Sum dy {mouseTrace.totalDy}
                </Typography>
              </Stack>
            </Stack>
          ) : (
            <Typography variant="body2" sx={{ color: "text.secondary" }}>
              No hay datos de raton para mostrar.
            </Typography>
          )}
        </DialogContent>
      </Dialog>
    </Stack>
  );
}
