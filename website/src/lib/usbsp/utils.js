export function clampInt(v, min, max, fallback) {
  const n = Number.parseInt(v, 10);
  if (Number.isNaN(n)) return fallback;
  return Math.max(min, Math.min(max, n));
}

export function parseHexInput(text) {
  const cleaned = String(text ?? "").replace(/[^0-9a-fA-F]/g, " ");
  const parts = cleaned.split(/\s+/).filter(Boolean);
  return new Uint8Array(parts.map((p) => Number.parseInt(p, 16) & 0xff));
}

export function hex4(n) {
  const v = Number(n) >>> 0;
  return (v & 0xffff).toString(16).padStart(4, "0");
}

export function sanitizeHex4Input(text) {
  const cleaned = String(text ?? "")
    .trim()
    .replace(/^0x/i, "")
    .replace(/[^0-9a-fA-F]/g, "")
    .slice(0, 4)
    .toLowerCase();
  return cleaned;
}

export function hasText(v) {
  return String(v ?? "").trim().length > 0;
}

