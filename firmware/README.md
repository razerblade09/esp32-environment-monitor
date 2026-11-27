# Firmware

**Status: In development.** Drivers for all sensors and the OLED are written and build in CI, but none has been run on real hardware yet.

## Toolchain

[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/) v5.3.x, edited in VS Code with the Espressif *ESP-IDF* extension. The extension installs the toolchain and provides a terminal with the environment already set up.

```
./build.sh                  # build; output: firmware/dist/esp32_environment_monitor.bin
./flash.sh /dev/ttyUSB0     # write that image to the ESP32 at offset 0x0
idf.py monitor              # serial console, 115200 baud
```

Run these from an ESP-IDF terminal (`idf.py` must be on `PATH`). `build.sh` produces a single merged image (bootloader + partition table + app), so that one file is all you upload.

Flashing needs a 3.3 V USB-UART adapter on header J1. Enter download mode by holding BOOT, tapping RESET, then releasing BOOT. See the top-level README for the header pinout.

Pin map, I2C addresses and open hardware questions: [`docs/FIRMWARE_INTERFACE.md`](../docs/FIRMWARE_INTERFACE.md).

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
    ├── i2c_bus/          # shared I2C master bus + address probe
    ├── bme280/           # temperature, humidity, pressure
    ├── scd40/            # CO2 (periodic measurement)
    ├── sgp40/            # VOC raw signal
    ├── pms5003/          # particulate matter UART frame parser
    └── ssd1306/          # 128x64 OLED text output
```

Rule: hardware access lives in components; `main/` only orchestrates.

## Roadmap

- [x] Project skeleton, pin map, I2C bus with boot-time device scan
- [x] BME280 driver (temperature, humidity, pressure)
- [x] SCD40 driver (CO2)
- [x] SGP40 driver (VOC)
- [x] PMS5003 UART frame parser
- [x] SSD1306 display output
- [x] Main sampling loop
- [ ] Test every driver on real hardware
- [ ] Discard PMS5003 readings during its ~30 s warm-up
- [ ] VOC index from the SGP40 raw signal (currently raw ticks only)
- [ ] (Optional) Wi-Fi telemetry / MQTT
