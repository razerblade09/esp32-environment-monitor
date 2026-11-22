# Firmware

**Status: In development.** Bring-up skeleton in place; sensor drivers are added one per pull request.

## Toolchain

[PlatformIO](https://platformio.org/) with the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/) framework, targeting the bare ESP32-WROOM-32 module.

```
pio run -d firmware                  # build
pio run -d firmware -t upload        # flash over the J1 USB-UART header
pio device monitor -d firmware       # serial console, 115200 baud
```

Flashing needs a 3.3 V USB-UART adapter on header J1 with GPIO0 held low (BOOT button) while resetting. See the top-level README for the header pinout.

## Layout

```
firmware/
├── platformio.ini        # build configuration
├── sdkconfig.defaults    # ESP-IDF project settings
├── src/                  # app_main: wires components together
└── components/
    ├── board/            # GPIO map and I2C addresses (mirrors the schematic)
    └── i2c_bus/          # shared I2C master bus + address probe
```

Rule: hardware access lives in components; `src/` only orchestrates.

## Roadmap

- [x] Project skeleton, pin map, I2C bus with boot-time device scan
- [ ] BME280 driver (temperature, humidity, pressure)
- [ ] SCD40 driver (CO2)
- [ ] SGP40 driver (VOC)
- [ ] PMS5003 UART frame parser
- [ ] SSD1306 display output
- [ ] Main sampling loop
- [ ] (Optional) Wi-Fi telemetry / MQTT
