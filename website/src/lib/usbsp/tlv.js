export function u16leBytes(n) {
  const v = Number(n) >>> 0;
  return new Uint8Array([v & 0xff, (v >>> 8) & 0xff]);
}

function encodeNibble(value) {
  const v = Number(value) >>> 0;
  if (v < 13) return { nibble: v, ext: [] };
  if (v < 269) return { nibble: 13, ext: [v - 13] };
  if (v < 65805) {
    const n = v - 269;
    return { nibble: 14, ext: [(n >>> 8) & 0xff, n & 0xff] };
  }
  throw new Error(`TLV field too large (${v})`);
}

function decodeNibble(nibble, buf, off) {
  if (nibble < 13) return { value: nibble, next: off };
  if (nibble === 13) {
    if (off + 1 > buf.length) return null;
    return { value: 13 + buf[off], next: off + 1 };
  }
  if (nibble === 14) {
    if (off + 2 > buf.length) return null;
    return { value: 269 + ((buf[off] << 8) | buf[off + 1]), next: off + 2 };
  }
  return null; // 15 is reserved
}

export function encodeTlv(type, valueBytes) {
  const v = valueBytes instanceof Uint8Array ? valueBytes : new Uint8Array(valueBytes ?? []);
  const t = encodeNibble(type);
  const l = encodeNibble(v.length);
  const out = new Uint8Array(1 + t.ext.length + l.ext.length + v.length);
  out[0] = (t.nibble & 0x0f) | ((l.nibble & 0x0f) << 4);
  let o = 1;
  for (const b of t.ext) out[o++] = b;
  for (const b of l.ext) out[o++] = b;
  out.set(v, o);
  return out;
}

export function packTlvs(tlvs, maxBytes) {
  const chunks = [];
  let cur = [];
  let curLen = 0;

  for (const tlv of tlvs) {
    if (!tlv?.length) continue;
    if (tlv.length > maxBytes) {
      throw new Error(`TLV too large (${tlv.length} bytes) for max payload ${maxBytes}`);
    }
    if (curLen + tlv.length > maxBytes) {
      chunks.push(cur);
      cur = [];
      curLen = 0;
    }
    cur.push(tlv);
    curLen += tlv.length;
  }
  if (cur.length) chunks.push(cur);

  return chunks.map((parts) => {
    const total = parts.reduce((n, p) => n + p.length, 0);
    const out = new Uint8Array(total);
    let o = 0;
    for (const p of parts) {
      out.set(p, o);
      o += p.length;
    }
    return out;
  });
}

export function parseTlvStore(eepromBytes) {
  const records = [];
  const buf = eepromBytes instanceof Uint8Array ? eepromBytes : new Uint8Array(eepromBytes ?? []);
  let off = 0;

  while (off < buf.length) {
    const h = buf[off++];
    if (h === 0xff) break; // erased EEPROM tail

    const t = decodeNibble(h & 0x0f, buf, off);
    if (!t) break;
    off = t.next;

    const l = decodeNibble((h >>> 4) & 0x0f, buf, off);
    if (!l) break;
    off = l.next;

    if (off + l.value > buf.length) break;
    if (t.value === 0 && l.value === 0) break; // terminator

    const value = buf.slice(off, off + l.value);
    const type = t.value & 0xff;
    records.push({ type, value });
    off += l.value;
  }

  return records;
}
