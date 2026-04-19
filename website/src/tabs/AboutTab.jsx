import React from "react";
import { Card, CardContent, CardHeader, Chip, Divider, Stack } from "@mui/material";

export default function AboutTab({ webVersion, versions }) {
  const cardSx = { overflow: "hidden" };
  const ch572dFwAvailable = !!versions?.ch572d;
  const ch572dFwLabel = `CH572D FW: ${versions?.ch572d ?? "N/A"}`;
  const ch32v203FwAvailable = !!versions?.ch32v203;
  const ch32v203FwLabel = `CH32V203 FW: ${versions?.ch32v203 ?? "N/A"}`;

  return (
    <Stack spacing={2}>
      <Card variant="outlined" sx={cardSx}>
        <CardHeader title="About USBsp" subheader="Installed firmware & web version" />
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
    </Stack>
  );
}
