function readU16le(buf, off) {
  return (buf[off] | (buf[off + 1] << 8)) >>> 0;
}

export function u16leBytes(n) {
  const v = Number(n) >>> 0;
  return new Uint8Array([v & 0xff, (v >>> 8) & 0xff]);
}

export function encodeTlv(type, valueBytes) {
  const v = valueBytes instanceof Uint8Array ? valueBytes : new Uint8Array(valueBytes ?? []);
  const sz = v.length & 0xffff;
  const out = new Uint8Array(3 + sz);
  out[0] = sz & 0xff;
  out[1] = (sz >>> 8) & 0xff;
  out[2] = type & 0xff;
  out.set(v, 3);
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

  while (off + 3 <= buf.length) {
    const size = readU16le(buf, off);
    const type = buf[off + 2];
    off += 3;
    if (size === 0 && type === 0x00) break; // terminator
    if (off + size > buf.length) break;
    const value = buf.slice(off, off + size);
    records.push({ type, value });
    off += size;
  }

  return records;
}

