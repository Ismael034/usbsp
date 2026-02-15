function decodeUsbIds(bytes) {
  const utf8 = new TextDecoder("utf-8", { fatal: false }).decode(bytes);
  const repl = (utf8.match(/\uFFFD/g) || []).length;
  if (repl > 16) return new TextDecoder("iso-8859-1").decode(bytes);
  return utf8;
}

export async function loadUsbIds({ signal } = {}) {
  const urls = [
    "https://usb-ids.gowdy.us/usb.ids",
    "https://raw.githubusercontent.com/vcrhonek/hwdata/master/usb.ids"
  ];

  let lastErr;
  for (const url of urls) {
    try {
      const res = await fetch(url, { signal, cache: "force-cache" });
      if (!res.ok) throw new Error(`HTTP ${res.status} for ${url}`);
      const buf = await res.arrayBuffer();
      const text = decodeUsbIds(new Uint8Array(buf));
      const parsed = parseUsbIds(text);
      return { ...parsed, sourceUrl: url };
    } catch (err) {
      lastErr = err;
    }
  }
  throw lastErr ?? new Error("Failed to load usb.ids");
}

export function parseUsbIds(text) {
  const vendors = new Map(); // vid -> vendorName
  const productsByVid = new Map(); // vid -> product[]

  let currentVid = null;
  let currentVendor = null;

  const lines = String(text ?? "").split(/\r?\n/);
  for (const line of lines) {
    if (!line || line[0] === "#") continue;

    if (line[0] === "C" && /\s/.test(line[1] ?? "")) continue;

    const mVendor = /^([0-9A-Fa-f]{4})\s+(.+)$/.exec(line);
    if (mVendor) {
      currentVid = mVendor[1].toLowerCase();
      currentVendor = mVendor[2].trim();
      vendors.set(currentVid, currentVendor);
      continue;
    }

    const mProd = /^\t([0-9A-Fa-f]{4})\s+(.+)$/.exec(line);
    if (mProd && currentVid && currentVendor) {
      const pid = mProd[1].toLowerCase();
      const productName = mProd[2].trim();
      const rec = {
        key: `${currentVid}:${pid}`,
        vid: currentVid,
        pid,
        vendorName: currentVendor,
        productName
      };
      if (!productsByVid.has(currentVid)) productsByVid.set(currentVid, []);
      productsByVid.get(currentVid).push(rec);
    }
  }

  return { vendors, productsByVid };
}

export function normHex4(v) {
  const s = String(v ?? "").trim().replace(/^0x/i, "");
  if (!s) return "";
  if (!/^[0-9a-fA-F]+$/.test(s)) return "";
  return s.toLowerCase().padStart(4, "0").slice(-4);
}
