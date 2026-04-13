async function withTimeout(promise, timeoutMs, label) {
  const ms = Number.isFinite(timeoutMs) ? timeoutMs : 0;
  if (ms <= 0) return await promise;
  let timeoutId = null;
  const timeout = new Promise((_, reject) => {
    timeoutId = setTimeout(() => reject(new Error(`${label}: timeout after ${ms}ms`)), ms);
  });
  try {
    return await Promise.race([promise, timeout]);
  } finally {
    if (timeoutId) clearTimeout(timeoutId);
  }
}

function copyUsbData(result) {
  const view = result?.data;
  if (!view) return null;
  return new Uint8Array(view.buffer, view.byteOffset, view.byteLength).slice();
}

export async function transferRawWithTimeout(device, epOut, epIn, payload, opts = {}) {
  if (!device) return null;
  const outTimeoutMs = Number.isFinite(opts.outTimeoutMs) ? opts.outTimeoutMs : 5000;
  const inLen = Number.isFinite(opts.inLen) ? opts.inLen : 64;

  await withTimeout(device.transferOut(epOut, payload), outTimeoutMs, "USB transferOut");
  const result = await device.transferIn(epIn, inLen);
  return copyUsbData(result);
}

export async function transferOutOnly(device, epOut, payload, opts = {}) {
  if (!device) return;
  const outTimeoutMs = Number.isFinite(opts.outTimeoutMs) ? opts.outTimeoutMs : 5000;
  await withTimeout(device.transferOut(epOut, payload), outTimeoutMs, "USB transferOut");
}
