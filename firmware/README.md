# Firmware

**Status: In development.** Bring-up skeleton in place; sensor drivers are added one per pull request.

## Toolchain

[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/) v5.3.x, edited in VS Code with the Espressif *ESP-IDF* extension. The extension installs the toolchain and provides a terminal with the environment already set up.

```
./build.sh                  # build; output: firmware/dist/esp32_environment_monitor.bin
./flash.sh /dev/ttyUSB0     # write that image to the ESP32 at offset 0x0
idf.py monitor              # serial console, 115200 baud
```

Run these from an ESP-IDF terminal (`idf.py` must be on `PATH`). `build.sh` produces a single merged image (bootloader + partition table + app), so that one file is all you upload.

Flashing needs a 3.3 V USB-UART adapter on header J1. Enter download mode by holding BOOT, tapping RESET, then releasing BOOT. See the top-level README for the header pinout.

## Layout

```
firmware/
├── build.sh              # build + merge into dist/
├── flash.sh              # upload dist/ image with esptool
├── CMakeLists.txt        # ESP-IDF project
├── sdkconfig.defaults    # project settings (generated sdkconfig is not tracked)
├── main/                 # app_main: wires components together
└── components/
    ├── board/            # GPIO map and I2C addresses (mirrors the schematic)
    └── i2c_bus/          # shared I2C master bus + address probe
```

Rule: hardware access lives in components; `main/` only orchestrates.

## Roadmap

- [x] Project skeleton, pin map, I2C bus with boot-time device scan
- [ ] BME280 driver (temperature, humidity, pressure)
- [ ] SCD40 driver (CO2)
- [ ] SGP40 driver (VOC)
- [ ] PMS5003 UART frame parser
- [ ] SSD1306 display output
- [ ] Main sampling loop
- [ ] (Optional) Wi-Fi telemetry / MQTT
