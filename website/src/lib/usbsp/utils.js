export function clampInt(v, min, max, fallback) {
  const n = Number.parseInt(v, 10);
  if (Number.isNaN(n)) return fallback;
  return Math.max(min, Math.min(max, n));
}

function bytesFromCompactHex(compact) {
  let text = compact;
  if (text.length % 2 === 1) {
    text = `0${text}`;
  }
  const out = [];
  for (let index = 0; index < text.length; index += 2) {
    out.push(Number.parseInt(text.slice(index, index + 2), 16) & 0xff);
  }
  return out;
}

export function validateRawHexInput(text) {
  const value = String(text ?? "").trim();
  if (!value) return "";
  try {
    parseHexInput(value);
    return "";
  } catch (err) {
    return err?.message ?? "Invalid hex input.";
  }
}

export function parseHexInput(text) {
  const value = String(text ?? "").trim();
  if (!value) return new Uint8Array();

  const tokens = value.split(/\s+/).filter(Boolean);
  const bytes = [];

  for (const token of tokens) {
    const cleaned = token.replace(/^0x/i, "");
    if (!cleaned || !/^[0-9a-fA-F]+$/.test(cleaned)) {
      throw new Error("Invalid hex input.");
    }
    bytes.push(...bytesFromCompactHex(cleaned));
  }

  return new Uint8Array(bytes);
}

export function hex4(n) {
  const v = Number(n) >>> 0;
  return (v & 0xffff).toString(16).padStart(4, "0");
}

export function sanitizeHex4Input(text) {
  return String(text ?? "")
    .trim()
    .replace(/^0x/i, "")
    .replace(/[^0-9a-fA-F]/g, "")
    .slice(0, 4)
    .toLowerCase();
}

export function hasText(v) {
  return String(v ?? "").trim().length > 0;
}
