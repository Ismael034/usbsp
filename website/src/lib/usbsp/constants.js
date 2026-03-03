export const VID = 0x1209;
export const PID = 0x0001;
export const IFACE = 2;
export const EP_OUT = 2;
export const EP_IN = 2;
export const EEPROM_SIZE = 256;

export const TLV_VID = 0x01;
export const TLV_PID = 0x02;
export const TLV_BCD_DEVICE = 0x03;
export const TLV_MAX_POWER_MA = 0x04;
export const TLV_FLAGS = 0x05;
export const TLV_ATTACH_DELAY_MS = 0x06;
export const TLV_CAPTURE_MAX_BYTES = 0x07;
export const TLV_MANUFACTURER = 0x08;
export const TLV_PRODUCT = 0x09;
export const TLV_SERIAL = 0x0a;

export const FLAG_SELF_POWERED = 1 << 0;
export const FLAG_REMOTE_WAKEUP = 1 << 1;
export const FLAG_BOOT_CONNECTED = 1 << 2;
export const FLAG_CAPTURE_ON_BOOT = 1 << 3;

