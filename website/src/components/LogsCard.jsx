import React, { useEffect, useRef } from "react";
import { Box, Button, Card, CardContent, CardHeader, Divider } from "@mui/material";

export default function LogsCard({ logText, onClear }) {
  const elRef = useRef(null);

  useEffect(() => {
    const el = elRef.current;
    if (!el) return;
    el.scrollTop = el.scrollHeight;
  }, [logText]);

  return (
    <Card variant="outlined" sx={{ height: "100%" }}>
      <CardHeader
        title="Logs"
        subheader="Device responses"
        action={
          <Button variant="text" size="small" onClick={onClear} disabled={!logText.length}>
            Clear logs
          </Button>
        }
      />
      <CardContent>
        <Divider sx={{ mb: 2 }} />
        <Box
          ref={elRef}
          component="pre"
          sx={{
            m: 0,
            p: 2,
            borderRadius: 2,
            minHeight: { xs: 320, md: 520 },
            maxHeight: { xs: 520, md: 720 },
            overflow: "auto",
            scrollbarGutter: "stable",
            backgroundColor: "#0b1220",
            color: "rgba(226,232,240,0.95)",
            border: "1px solid rgba(226,232,240,0.10)",
            fontFamily: '"JetBrains Mono","Fira Code",monospace',
            fontSize: 12,
            whiteSpace: "pre-wrap",
            overflowWrap: "anywhere",
            wordBreak: "break-word",
            scrollbarWidth: "thin",
            scrollbarColor: "rgba(226,232,240,0.26) rgba(0,0,0,0)",
            "&::-webkit-scrollbar": { width: 10, height: 10 },
            "&::-webkit-scrollbar-track": { background: "rgba(0,0,0,0)" },
            "&::-webkit-scrollbar-thumb": {
              backgroundColor: "rgba(226,232,240,0.22)",
              borderRadius: 10,
              border: "2px solid #0b1220"
            },
            "&::-webkit-scrollbar-thumb:hover": { backgroundColor: "rgba(226,232,240,0.32)" }
          }}
        >
          {logText}
        </Box>
      </CardContent>
    </Card>
  );
}

