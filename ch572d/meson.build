inc_dirs = include_directories(
    'src/src',
    'src/src/eeprom',
    'src/src/usb',
    'src/src/external/microtlv',
    'src/StdPeriphDriver/inc',
    'src/RVMSIS'
)

firmware_main = 'src/src/main.c'

firmware_sources = files(
    # startup
    'src/Startup/startup_CH572.S',
    # src directory
    'src/src/eeprom/eeprom.c',
    'src/src/log.c',
    'src/src/spi_link.c',
    'src/src/external/microtlv/tlv.c',
    'src/src/usb/usb_cdc.c',
    'src/src/usb/usb_logs.c',
    'src/src/usb/usb_webusb.c',
    'src/src/usb/usb_webusb_cmds.c',
    'src/src/usb/webusb_cmds/webusb_cmd_eeprom_read.c',
    'src/src/usb/webusb_cmds/webusb_cmd_eeprom_write.c',
    'src/src/usb/webusb_cmds/webusb_cmd_ch32_reset.c',
    'src/src/usb/webusb_cmds/webusb_cmd_get_versions.c',
    'src/src/usb/webusb_cmds/webusb_cmd_get_active_usb_info.c',
    'src/src/usb/webusb_cmds/webusb_cmd_capture_poll.c',
    'src/src/usb/webusb_cmds/webusb_cmd_spi_pingpong.c',
    'src/src/usb/webusb_cmds/webusb_cmd_spi_text.c',
    # StdPeriphDriver
    'src/StdPeriphDriver/CH57x_clk.c',
    'src/StdPeriphDriver/CH57x_cmp.c',
    'src/StdPeriphDriver/CH57x_flash.c',
    'src/StdPeriphDriver/CH57x_gpio.c',
    'src/StdPeriphDriver/CH57x_i2c.c',
    'src/StdPeriphDriver/CH57x_keyscan.c',
    'src/StdPeriphDriver/CH57x_pwm.c',
    'src/StdPeriphDriver/CH57x_pwr.c',
    'src/StdPeriphDriver/CH57x_spi.c',
    'src/StdPeriphDriver/CH57x_sys.c',
    'src/StdPeriphDriver/CH57x_timer.c',
    'src/StdPeriphDriver/CH57x_uart.c',
    'src/StdPeriphDriver/CH57x_usbdev.c',
    'src/StdPeriphDriver/CH57x_usbhostBase.c',
    'src/StdPeriphDriver/CH57x_usbhostClass.c'
)

linker_script = meson.current_source_dir() + '/src/Ld/Link.ld'
isp572_lib_dir = meson.current_source_dir() + '/src/StdPeriphDriver'

firmware_exe = executable('ch572d',
    sources: [ firmware_main, firmware_sources ],
    include_directories: inc_dirs,
    c_args: ['-DDEBUG=1', '-std=gnu99'],
    link_args: [
        '-T', linker_script,
        '-L' + isp572_lib_dir,
        '-lISP572',
        '-lm',
    ],
    name_suffix: 'elf'
)

firmware_hex = custom_target(
    'ch572d.hex',
    input: firmware_exe,
    output: 'ch572d.hex',
    command: ['riscv-none-embed-objcopy', '-O', 'ihex', '@INPUT@', '@OUTPUT@'],
)

firmware_bin = custom_target(
    'ch572d.bin',
    input: firmware_exe,
    output: 'ch572d.bin',
    command: ['riscv-none-embed-objcopy', '-O', 'binary', '@INPUT@', '@OUTPUT@'],
)
