function asSigned8(value = 0) {
  return value > 127 ? value - 256 : value;
}

function asSigned16le(low = 0, high = 0) {
  const value = (low & 0xff) | ((high & 0xff) << 8);
  return value > 0x7fff ? value - 0x10000 : value;
}

function isSignByte(value) {
  return value === 0x00 || value === 0xff;
}

function splitMouseReport(bytes) {
  if (!bytes?.length) {
    return { reportId: null, body: bytes ?? [] };
  }

  if (bytes.length >= 6 && bytes[0] > 0 && bytes[0] <= 0x0f) {
    return { reportId: bytes[0], body: bytes.slice(1) };
  }

  return { reportId: null, body: bytes };
}

function listButtons(mask) {
  const buttons = [];
  if (mask & 0x01) buttons.push("Left");
  if (mask & 0x02) buttons.push("Right");
  if (mask & 0x04) buttons.push("Middle");
  if (mask & 0x08) buttons.push("Back");
  if (mask & 0x10) buttons.push("Forward");
  return buttons;
}

function looksLikeWideAxesReport(body, forceWideAxes) {
  if (forceWideAxes) {
    return body.length >= 5;
  }

  if (body.length !== 6 && body.length !== 7) {
    return false;
  }

  if (!isSignByte(body[2]) || !isSignByte(body[4])) {
    return false;
  }

  return asSigned16le(body[1], body[2]) !== 0 || asSigned16le(body[3], body[4]) !== 0;
}

export function parseHidMouseReport(bytes, { forceWideAxes = false } = {}) {
  if (!bytes?.length) return null;

  const { reportId, body } = splitMouseReport(bytes);

  if (body.length === 1) {
    const delta = asSigned8(body[0]);
    return {
      reportId,
      buttons: [],
      dx: 0,
      dy: 0,
      wheel: delta,
      pan: 0,
      format: "delta",
      idle: delta === 0
    };
  }

  if (body.length === 2) {
    return {
      reportId,
      buttons: [],
      dx: asSigned8(body[0]),
      dy: asSigned8(body[1]),
      wheel: 0,
      pan: 0,
      format: "xy-8",
      idle: body[0] === 0 && body[1] === 0
    };
  }

  if (body.length < 3) return null;

  const buttons = listButtons(body[0] ?? 0);

  if (looksLikeWideAxesReport(body, forceWideAxes)) {
    const dx16 = asSigned16le(body[1], body[2]);
    const dy16 = asSigned16le(body[3], body[4]);
    const wheel16 = asSigned8(body[5] ?? 0);
    const pan16 = asSigned8(body[6] ?? 0);

    return {
      reportId,
      buttons,
      dx: dx16,
      dy: dy16,
      wheel: wheel16,
      pan: pan16,
      format: "xy-16",
      idle: buttons.length === 0 && dx16 === 0 && dy16 === 0 && wheel16 === 0 && pan16 === 0
    };
  }

  let dx = asSigned8(body[1] ?? 0);
  let dy = asSigned8(body[2] ?? 0);
  let wheel = asSigned8(body[3] ?? 0);
  let pan = asSigned8(body[4] ?? 0);
  let usedAltAxes = false;

  // Some non-boot mice place movement in bytes 3/4 while bytes 1/2 stay zero.
  if (body.length >= 5 && dx === 0 && dy === 0) {
    const altDx = asSigned8(body[3]);
    const altDy = asSigned8(body[4]);
    if (altDx !== 0 || altDy !== 0) {
      dx = altDx;
      dy = altDy;
      wheel = 0;
      pan = 0;
      usedAltAxes = true;
    }
  }

  return {
    reportId,
    buttons,
    dx,
    dy,
    wheel,
    pan,
    format: usedAltAxes ? "xy-8-alt" : "boot",
    idle: buttons.length === 0 && dx === 0 && dy === 0 && wheel === 0 && pan === 0
  };
}

export function decodeHidMouse(bytes, { forceWideAxes = false } = {}) {
  const parsed = parseHidMouseReport(bytes, { forceWideAxes });
  if (!parsed) return bytes?.length ? "Not a boot-mouse report" : "";

  if (parsed.format === "delta") {
    return parsed.wheel === 0 ? "Mouse: idle" : `Mouse: delta ${parsed.wheel}`;
  }

  const parts = [];
  if (parsed.reportId != null) parts.push(`report ${parsed.reportId}`);
  if (parsed.buttons.length) parts.push(`buttons ${parsed.buttons.join(", ")}`);
  if (parsed.dx !== 0) parts.push(`x ${parsed.dx}`);
  if (parsed.dy !== 0) parts.push(`y ${parsed.dy}`);
  if (parsed.wheel !== 0) parts.push(`wheel ${parsed.wheel}`);
  if (parsed.pan !== 0) parts.push(`pan ${parsed.pan}`);

  return parts.length ? `Mouse: ${parts.join(" | ")}` : "Mouse: idle";
}
