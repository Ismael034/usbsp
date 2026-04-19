import React, { useEffect, useMemo, useRef, useState } from "react";
import {
  Box,
  Button,
  CircularProgress,
  Dialog,
  DialogContent,
  DialogTitle,
  Grid,
  IconButton,
  InputAdornment,
  List,
  ListItemButton,
  ListItemText,
  TextField,
  Typography
} from "@mui/material";
import SearchIcon from "@mui/icons-material/Search";
import CloseIcon from "@mui/icons-material/Close";
import ClearIcon from "@mui/icons-material/Clear";
import { loadUsbIds } from "../usbids.js";
import { hasText } from "../lib/usbsp/utils.js";

const USB_LOOKUP_PAGE_SIZE = 100;
const USB_LOOKUP_PRELOAD_PX = 260;

export default function UsbLookupDialog({ open, onClose, onApply }) {
  const [usbLookupQuery, setUsbLookupQuery] = useState("");
  const [usbLookupVendor, setUsbLookupVendor] = useState(null); // { vid, name } | null
  const [usbLookupProduct, setUsbLookupProduct] = useState(null); // { vid, pid, vendorName, productName } | null
  const [usbLookupProductQuery, setUsbLookupProductQuery] = useState("");
  const [usbVendorVisibleCount, setUsbVendorVisibleCount] = useState(USB_LOOKUP_PAGE_SIZE);
  const [usbProductVisibleCount, setUsbProductVisibleCount] = useState(USB_LOOKUP_PAGE_SIZE);
  const usbLookupVendorInputRef = useRef(null);
  const usbLookupProductInputRef = useRef(null);
  const usbLookupAbortRef = useRef(null);

  const [usbIds, setUsbIds] = useState({
    status: "idle", // idle|loading|ready|error
    vendors: null,
    productsByVid: null,
    sourceUrl: null,
    error: null
  });

  const usbVendorsArray = useMemo(() => {
    if (usbIds.status !== "ready" || !usbIds.vendors || !usbIds.productsByVid) return [];
    const arr = [];
    for (const [vid, name] of usbIds.vendors.entries()) {
      if (!vid || !name) continue;
      const list = usbIds.productsByVid.get(vid);
      if (!list || list.length === 0) continue;
      arr.push({ vid, name });
    }
    arr.sort((a, b) => a.name.localeCompare(b.name));
    return arr;
  }, [usbIds]);

  const usbVendorsTotal = usbVendorsArray.length;

  const usbVendorMatches = useMemo(() => {
    const q = String(usbLookupQuery ?? "").trim().toLowerCase().replace(/^0x/, "");
    if (!q) return usbVendorsArray;
    return usbVendorsArray
      .filter((v) => v.vid.includes(q) || v.name.toLowerCase().includes(q));
  }, [usbLookupQuery, usbVendorsArray]);
  const usbVendorMatchesTotal = usbVendorMatches.length;

  const usbVendorResults = useMemo(
    () => usbVendorMatches.slice(0, usbVendorVisibleCount),
    [usbVendorMatches, usbVendorVisibleCount]
  );

  const usbProductsTotalForVendor = useMemo(() => {
    if (usbIds.status !== "ready" || !usbIds.productsByVid || !usbLookupVendor?.vid) return 0;
    return (usbIds.productsByVid.get(usbLookupVendor.vid) ?? []).length;
  }, [usbIds, usbLookupVendor]);

  const usbProductMatches = useMemo(() => {
    if (usbIds.status !== "ready" || !usbIds.productsByVid || !usbLookupVendor?.vid) return [];
    const list = usbIds.productsByVid.get(usbLookupVendor.vid) ?? [];
    const q = String(usbLookupProductQuery ?? "").trim().toLowerCase().replace(/^0x/, "");
    if (!q) return list;
    const wantPid = /^[0-9a-f]{1,4}$/.test(q) ? q.padStart(4, "0") : null;
    return list
      .filter((p) => {
        if (wantPid && p.pid === wantPid) return true;
        return (
          p.pid.includes(q) ||
          p.productName.toLowerCase().includes(q) ||
          p.vendorName.toLowerCase().includes(q)
        );
      });
  }, [usbIds, usbLookupVendor, usbLookupProductQuery]);
  const usbProductMatchesTotal = usbProductMatches.length;

  const usbProductResults = useMemo(
    () => usbProductMatches.slice(0, usbProductVisibleCount),
    [usbProductMatches, usbProductVisibleCount]
  );

  const handlePagedListScroll = (event, visibleCount, totalCount, setVisibleCount) => {
    const node = event.currentTarget;
    if (visibleCount >= totalCount) return;
    if (node.scrollTop + node.clientHeight < node.scrollHeight - USB_LOOKUP_PRELOAD_PX) return;
    setVisibleCount((prev) => Math.min(prev + USB_LOOKUP_PAGE_SIZE, totalCount));
  };

  useEffect(() => {
    setUsbVendorVisibleCount(USB_LOOKUP_PAGE_SIZE);
  }, [usbLookupQuery, open]);

  useEffect(() => {
    setUsbProductVisibleCount(USB_LOOKUP_PAGE_SIZE);
  }, [usbLookupProductQuery, usbLookupVendor, open]);

  useEffect(() => {
    if (!open) return;
    const t = setTimeout(() => usbLookupVendorInputRef.current?.focus?.(), 0);
    return () => clearTimeout(t);
  }, [open]);

  useEffect(() => {
    if (!open) return;
    if (usbIds.status === "ready" || usbIds.status === "loading") return;

    const ac = new AbortController();
    usbLookupAbortRef.current?.abort?.();
    usbLookupAbortRef.current = ac;

    setUsbIds({ status: "loading", vendors: null, productsByVid: null, sourceUrl: null, error: null });
    loadUsbIds({ signal: ac.signal })
      .then((r) => {
        setUsbIds({
          status: "ready",
          vendors: r.vendors,
          productsByVid: r.productsByVid,
          sourceUrl: r.sourceUrl,
          error: null
        });
      })
      .catch((err) => {
        if (ac.signal.aborted) return;
        setUsbIds({
          status: "error",
          vendors: null,
          productsByVid: null,
          sourceUrl: null,
          error: err?.message ?? String(err)
        });
      });

    return () => ac.abort();
    // Intentionally depends only on `open`: including `usbIds.status` would abort the in-flight
    // fetch as soon as we set status="loading".
  }, [open]);

  return (
    <Dialog open={open} onClose={onClose} fullWidth maxWidth="md">
      <DialogTitle sx={{ display: "flex", alignItems: "center", gap: 1 }}>
        <SearchIcon fontSize="small" />
        <Box sx={{ flex: 1, minWidth: 0 }}>
          Search USB IDs
          <Typography variant="body2" sx={{ color: "text.secondary" }}>
            Step 1: select vendor (VID). Step 2: select product (PID).
          </Typography>
        </Box>
        <IconButton aria-label="close" onClick={onClose}>
          <CloseIcon />
        </IconButton>
      </DialogTitle>

      <DialogContent dividers>
        <Grid container spacing={2}>
          <Grid item xs={12} md={5}>
            <TextField
              fullWidth
              value={usbLookupQuery}
              onChange={(e) => setUsbLookupQuery(e.target.value)}
              placeholder="Filter vendors (e.g. logitech, 046d)"
              size="small"
              inputRef={usbLookupVendorInputRef}
              type="text"
              InputProps={{
                startAdornment: (
                  <InputAdornment position="start">
                    <SearchIcon fontSize="small" />
                  </InputAdornment>
                ),
                endAdornment: (
                  <InputAdornment position="end">
                    {usbLookupQuery ? (
                      <IconButton
                        size="small"
                        aria-label="clear vendor filter"
                        onClick={() => {
                          setUsbLookupQuery("");
                          usbLookupVendorInputRef.current?.focus?.();
                        }}
                      >
                        <ClearIcon fontSize="small" />
                      </IconButton>
                    ) : null}
                    {usbIds.status === "loading" ? <CircularProgress size={18} /> : null}
                  </InputAdornment>
                )
              }}
            />
          </Grid>

          <Grid item xs={12} md={7}>
            <TextField
              fullWidth
              value={usbLookupProductQuery}
              onChange={(e) => setUsbLookupProductQuery(e.target.value)}
              placeholder={
                usbLookupVendor
                  ? `Filter products for VID ${usbLookupVendor.vid.toUpperCase()} (e.g. 6001, uart)`
                  : "Select a vendor to search products"
              }
              size="small"
              disabled={!usbLookupVendor}
              inputRef={usbLookupProductInputRef}
              type="text"
              InputProps={{
                startAdornment: (
                  <InputAdornment position="start">
                    <SearchIcon fontSize="small" />
                  </InputAdornment>
                ),
                endAdornment: usbLookupProductQuery ? (
                  <InputAdornment position="end">
                    <IconButton
                      size="small"
                      aria-label="clear product filter"
                      onClick={() => {
                        setUsbLookupProductQuery("");
                        usbLookupProductInputRef.current?.focus?.();
                      }}
                    >
                      <ClearIcon fontSize="small" />
                    </IconButton>
                  </InputAdornment>
                ) : null
              }}
            />
          </Grid>
        </Grid>

        {usbIds.status === "error" && (
          <Box sx={{ mt: 2, p: 1.5, border: "1px solid #d7dbe3", borderRadius: 2 }}>
            <Typography variant="body2" sx={{ color: "text.secondary" }}>
              Failed to load USB database: {usbIds.error}
            </Typography>
          </Box>
        )}

        <Grid container spacing={2} sx={{ mt: 0.5 }}>
          <Grid item xs={12} md={5}>
            <Typography variant="subtitle2" sx={{ mb: 0.5 }}>
              Vendors
            </Typography>
            <Box sx={{ border: "1px solid #d7dbe3", borderRadius: 2, overflow: "hidden" }}>
              <List
                dense
                disablePadding
                onScroll={(event) =>
                  handlePagedListScroll(
                    event,
                    usbVendorResults.length,
                    usbVendorMatches.length,
                    setUsbVendorVisibleCount
                  )
                }
                sx={{ maxHeight: 360, overflow: "auto", "& .MuiListItemButton-root": { transition: "none" } }}
              >
                {usbVendorResults.map((v) => (
                  <ListItemButton
                    key={v.vid}
                    disableRipple
                    onClick={() => {
                      setUsbLookupVendor({ vid: v.vid, name: v.name });
                      setUsbLookupProduct(null);
                      setUsbLookupProductQuery("");
                    }}
                    selected={usbLookupVendor?.vid === v.vid}
                  >
                    <ListItemText primary={`${v.name}`} secondary={`VID ${v.vid.toUpperCase()}`} />
                  </ListItemButton>
                ))}
                {!usbVendorResults.length && (
                  <ListItemText primary="No results" secondary="Type to search" sx={{ px: 2, py: 1.5 }} />
                )}
              </List>
            </Box>
            <Typography variant="caption" sx={{ display: "block", mt: 0.75, color: "text.secondary" }}>
              {usbLookupQuery
                ? `${usbVendorMatchesTotal} result${usbVendorMatchesTotal === 1 ? "" : "s"}`
                : `${usbVendorsTotal} vendors`}
            </Typography>
          </Grid>

          <Grid item xs={12} md={7}>
            <Typography variant="subtitle2" sx={{ mb: 0.5 }}>
              Products
            </Typography>
            <Box sx={{ border: "1px solid #d7dbe3", borderRadius: 2, overflow: "hidden" }}>
              <List
                dense
                disablePadding
                onScroll={(event) =>
                  handlePagedListScroll(
                    event,
                    usbProductResults.length,
                    usbProductMatches.length,
                    setUsbProductVisibleCount
                  )
                }
                sx={{ maxHeight: 360, overflow: "auto", "& .MuiListItemButton-root": { transition: "none" } }}
              >
                {!usbLookupVendor && (
                  <ListItemText
                    primary="Select a vendor"
                    secondary="Products appear here after selecting a vendor"
                    sx={{ px: 2, py: 1.5 }}
                  />
                )}

                {usbLookupVendor &&
                  usbProductResults.map((p) => (
                    <ListItemButton
                      key={p.key}
                      disableRipple
                      onClick={() => {
                        setUsbLookupProduct({
                          key: p.key,
                          vid: p.vid,
                          pid: p.pid,
                          vendorName: p.vendorName,
                          productName: p.productName
                        });
                      }}
                      selected={usbLookupProduct?.vid === p.vid && usbLookupProduct?.pid === p.pid}
                      sx={{
                        "&.Mui-selected": { bgcolor: "rgba(11, 95, 255, 0.10)" },
                        "&.Mui-selected:hover": { bgcolor: "rgba(11, 95, 255, 0.14)" }
                      }}
                    >
                      <ListItemText
                        primary={`${p.productName}`}
                        secondary={`${p.vid.toUpperCase()}:${p.pid.toUpperCase()}`}
                      />
                    </ListItemButton>
                  ))}

                {usbLookupVendor && !usbProductResults.length && (
                  <ListItemText
                    primary="No results"
                    secondary="Try a different filter"
                    sx={{ px: 2, py: 1.5 }}
                  />
                )}
              </List>
            </Box>
            {usbLookupVendor && (
              <Typography variant="caption" sx={{ display: "block", mt: 0.75, color: "text.secondary" }}>
                {usbLookupProductQuery
                  ? `${usbProductMatchesTotal} result${usbProductMatchesTotal === 1 ? "" : "s"}`
                  : `${usbProductsTotalForVendor} products`}
              </Typography>
            )}
          </Grid>
        </Grid>
      </DialogContent>

      <Box sx={{ display: "flex", gap: 1, justifyContent: "flex-end", p: 2, borderTop: "1px solid #d7dbe3" }}>
        <Button variant="text" onClick={onClose}>
          Cancel
        </Button>
        <Button
          variant="contained"
          disabled={!usbLookupVendor}
          onClick={() => {
            const vendor = usbLookupVendor;
            const prod = usbLookupProduct;
            if (!vendor) return;

            const vendorName = hasText(vendor?.name) ? vendor.name : null;
            const prodVendorName = hasText(prod?.vendorName) ? prod.vendorName : null;
            const manufacturer = vendorName || prodVendorName || "N/A";

            if (prod) {
              onApply?.({
                vid: prod.vid,
                pid: prod.pid,
                manufacturer,
                product: hasText(prod.productName) ? prod.productName : "N/A"
              });
            } else {
              onApply?.({ vid: vendor.vid, pid: "", manufacturer, product: null });
            }
            onClose?.();
          }}
        >
          Apply
        </Button>
      </Box>
    </Dialog>
  );
}
