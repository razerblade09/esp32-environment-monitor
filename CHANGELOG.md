# Changelog

All notable changes to this project are documented here.

---

## [Unreleased]

### In Progress
- Footprint assignment for all schematic components

---

## [0.1.0] — 2026-11-21

### Added
- KiCad schematic with all components placed and connected
- Custom KiCad symbols: PMS5003, SGP40, SSD1306 OLED
- ESP32-WROOM-32 bare module integration
- BME280 temperature/humidity/pressure sensor (I²C, address 0x76)
- SGP40 VOC sensor (I²C)
- SCD40 CO₂ sensor (I²C)
- SSD1306 OLED display (I²C)
- PMS5003 particulate matter sensor (UART2: GPIO16/GPIO17)
- I²C bus on GPIO21 (SDA) and GPIO22 (SCL)
- ESP32 boot (GPIO0) and reset (EN) circuitry with push buttons
- AMS1117-3.3 power supply circuit (5V → 3.3V)
- Decoupling capacitors on 5V and 3.3V rails
- USB-UART programming header (J1)
- PWR_FLAG symbols on power nets
- ERC run: 0 errors, 1 known benign warning (BME280 SDO / PWR_FLAG pin-type mismatch)
- Project documentation: README, Handover.txt, CHANGELOG
- Repository structure: `hardware/`, `docs/`, `firmware/`
