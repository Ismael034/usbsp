export async function transferRawWithTimeout(device, epOut, epIn, payload, opts = {}) {
  if (!device) return null;

  const outTimeoutMs = Number.isFinite(opts.outTimeoutMs) ? opts.outTimeoutMs : 5000;
  const inTimeoutMs = Number.isFinite(opts.inTimeoutMs) ? opts.inTimeoutMs : 20000;
  const inLen = Number.isFinite(opts.inLen) ? opts.inLen : 64;

  const withTimeout = async (p, timeoutMs, label) => {
    const ms = Number.isFinite(timeoutMs) ? timeoutMs : 0;
    if (ms <= 0) return await p;
    let t = null;
    const timeout = new Promise((_, reject) => {
      t = setTimeout(() => reject(new Error(`${label}: timeout after ${ms}ms`)), ms);
    });
    try {
      return await Promise.race([p, timeout]);
    } finally {
      if (t) clearTimeout(t);
    }
  };

  await withTimeout(device.transferOut(epOut, payload), outTimeoutMs, "USB transferOut");
  const result = await withTimeout(device.transferIn(epIn, inLen), inTimeoutMs, "USB transferIn");
  return new Uint8Array(result.data.buffer);
}

