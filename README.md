# usbsp

A work in progress for a (really) cheap embedded USB packet analyzer. CURRENTLY NOT FUNCTIONAL

## Builing

The required toolchain is alredy included. It is a custom toolchain made by WCH, downloaded from [here](https://www.mounriver.com/download). This toolchain constains custom targets for their MCUs. Other toolchains (like xpack) have not been tested.

### Clone the repository

```
git clone https://github.com/Ismael034/usbsp.git
cd usbsp
```

### Configure meson

```
meson setup build-ch32v203 --cross-file ch32v203/ch32v203-cross.ini
meson setup build-ch572d --cross-file ch572d/ch572d-cross.ini
```

### Build with ninja

```
ninja -C build-ch32v203 ch32v203/ch32v203.hex
ninja -C build-ch572d ch572d/ch572d.hex
```

If everything goes well, you'll end up with two .hex files – one for each MCU.

## Flashing

To flash the firmware to your MCU, you can use one of the following tools:

- [WCH-LinkUtility](https://www.wch.cn/downloads/wch-linkutility_zip.html)
- [wlink](https://github.com/ch32-rs/wlink/)
- [OpenOCD](https://openocd.org/)

```
wlink flash ./build-ch32v203/ch32v203/ch32v203.hex
wlink flash ./build-ch572d/ch572d/ch572d.hex
```

## Tests

It is also possible to build a test firmware, which will test hardware funcionality like EEPROM access, USBD, USBH, buttons, etc. You can build it using a predefined custom target.

```
ninja -C build-ch32v203 ch32v203/ch32v203_test.hex
ninja -C build-ch572d ch572d/ch572d_test.hex
```
