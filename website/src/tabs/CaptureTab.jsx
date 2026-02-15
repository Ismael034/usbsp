import React from "react";
import { Box, Button, Card, CardContent, CardHeader, Chip, Divider, Stack, Typography } from "@mui/material";
import ScienceIcon from "@mui/icons-material/Science";

export default function CaptureTab({
  captureRunning,
  packetsLen,
  disableActions,
  busy,
  connected,
  onStart,
  onStop,
  onClearPackets
}) {
  const cardSx = { overflow: "hidden" };

  return (
    <Stack spacing={2}>
      <Card variant="outlined" sx={cardSx}>
        <CardHeader
          title="USB Packet Capture"
          subheader="Stream captured packets from CH572D via SPI (relayed to the browser over WebUSB)"
        />
        <CardContent>
          <Divider sx={{ mb: 2 }} />
          <Stack direction="row" spacing={1} useFlexGap flexWrap="wrap">
            <Chip
              label={captureRunning ? "Capture Running" : "Capture Stopped"}
              color={captureRunning ? "success" : "default"}
              variant="filled"
            />
            <Button
              variant="contained"
              startIcon={<ScienceIcon />}
              onClick={onStart}
              disabled={disableActions || captureRunning}
            >
              Start
            </Button>
            <Button
              variant="outlined"
              onClick={onStop}
              disabled={busy || !connected || !captureRunning}
            >
              Stop
            </Button>
            <Button variant="text" onClick={onClearPackets} disabled={!packetsLen}>
              Clear Packets
            </Button>
          </Stack>

          <Box
            sx={{
              mt: 2,
              border: "1px solid",
              borderColor: "divider",
              borderRadius: 2,
              p: 1.5,
              bgcolor: "background.paper",
              color: "text.secondary"
            }}
          >
            <Typography variant="body2">
              Packet rendering is not implemented yet. Once the firmware protocol is finalized, this
              panel will show a live stream with filtering and export.
            </Typography>
          </Box>
        </CardContent>
      </Card>
    </Stack>
  );
}
