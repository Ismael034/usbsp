inc_dirs = include_directories(
    'src/src',
    'src/src/eeprom',
    'src/src/usb',
    'src/src/usb/usbd',
    'src/src/usb/usbh',
    'src/vendor/Core',
    'src/vendor/Debug',
    'src/vendor/Peripheral/inc',
    'src/vendor/User',
    'src/vendor/User/USB-Driver/inc'
)

firmware_main = 'src/src/main.c'
firmware_main_test = 'tests/src/main.c'

firmware_sources = files(
    # src directory
    'src/src/eeprom/eeprom.c',
    'src/src/system.c',
    'src/src/usb/relay.c',
    'src/src/usb/usb_relay.c',
    'src/src/usb/usbd/usb_desc.c',
    'src/src/usb/usbd/usb_endp.c',
    'src/src/usb/usbd/usb_hw.c',
    'src/src/usb/usbd/usb_istr.c',
    'src/src/usb/usbd/usb_prop.c',
    'src/src/usb/usbd/usb_pwr.c',
    'src/src/usb/usbd/usbd.c',
    'src/src/usb/usbh/app.c',
    'src/src/usb/usbh/usb_hid.c',
    'src/src/usb/usbh/usb_hub.c',
    'src/src/usb/usbh/usb_hw.c',
    'src/src/usb/usbh/usbh.c',
    'src/src/user.c',
    # vendor directory
    'src/vendor/Core/core_riscv.c',
    'src/vendor/Debug/debug.c',
    'src/vendor/Peripheral/src/ch32v20x_adc.c',
    'src/vendor/Peripheral/src/ch32v20x_bkp.c',
    'src/vendor/Peripheral/src/ch32v20x_can.c',
    'src/vendor/Peripheral/src/ch32v20x_crc.c',
    'src/vendor/Peripheral/src/ch32v20x_dbgmcu.c',
    'src/vendor/Peripheral/src/ch32v20x_dma.c',
    'src/vendor/Peripheral/src/ch32v20x_exti.c',
    'src/vendor/Peripheral/src/ch32v20x_flash.c',
    'src/vendor/Peripheral/src/ch32v20x_gpio.c',
    'src/vendor/Peripheral/src/ch32v20x_i2c.c',
    'src/vendor/Peripheral/src/ch32v20x_iwdg.c',
    'src/vendor/Peripheral/src/ch32v20x_misc.c',
    'src/vendor/Peripheral/src/ch32v20x_opa.c',
    'src/vendor/Peripheral/src/ch32v20x_pwr.c',
    'src/vendor/Peripheral/src/ch32v20x_rcc.c',
    'src/vendor/Peripheral/src/ch32v20x_rtc.c',
    'src/vendor/Peripheral/src/ch32v20x_spi.c',
    'src/vendor/Peripheral/src/ch32v20x_tim.c',
    'src/vendor/Peripheral/src/ch32v20x_usart.c',
    'src/vendor/Peripheral/src/ch32v20x_wwdg.c',
    'src/vendor/User/ch32v20x_it.c',
    'src/vendor/User/system_ch32v20x.c',
    'src/vendor/User/USB-Driver/src/usb_core.c',
    'src/vendor/User/USB-Driver/src/usb_init.c',
    'src/vendor/User/USB-Driver/src/usb_int.c',
    'src/vendor/User/USB-Driver/src/usb_mem.c',
    'src/vendor/User/USB-Driver/src/usb_regs.c',
    'src/vendor/User/USB-Driver/src/usb_sil.c',
    'src/vendor/Startup/startup_ch32v20x_D6.S',
)

linker_script = meson.current_source_dir() + '/ch32v203.ld'

# Firmware ELF executable
firmware_exe = executable('ch32v203',
    sources: [ firmware_main, firmware_sources ],
    include_directories: inc_dirs,
    link_args: ['-T', linker_script],
    name_suffix: 'elf'
)

# Test ELF executable
firmware_test_exe = executable('ch32v203_test',
    sources: [ firmware_main_test, firmware_sources ],
    include_directories: inc_dirs,
    link_args: ['-T', linker_script],
    name_suffix: 'elf'
)

# Generate HEX for firmware
firmware_hex = custom_target(
    'ch32v203.hex',
    input: firmware_exe,
    output: 'ch32v203.hex',
    command: ['riscv-none-embed-objcopy', '-O', 'ihex', '@INPUT@', '@OUTPUT@'],
)

# Generate BIN for firmware
firmware_bin = custom_target(
    'ch32v203.bin',
    input: firmware_exe,
    output: 'ch32v203.bin',
    command: ['riscv-none-embed-objcopy', '-O', 'binary', '@INPUT@', '@OUTPUT@'],
)

# Generate HEX for test executable
firmware_test_hex = custom_target(
    'ch32v203_test.hex',
    input: firmware_test_exe,
    output: 'ch32v203_test.hex',
    command: ['riscv-none-embed-objcopy', '-O', 'ihex', '@INPUT@', '@OUTPUT@'],
)

# Generate BIN for test executable
firmware_test_bin = custom_target(
    'ch32v203_test.bin',
    input: firmware_test_exe,
    output: 'ch32v203_test.bin',
    command: ['riscv-none-embed-objcopy', '-O', 'binary', '@INPUT@', '@OUTPUT@'],
)