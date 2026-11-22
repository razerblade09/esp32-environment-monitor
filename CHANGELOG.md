# Changelog

All notable changes to this project are documented here.

---

## [Unreleased]

### Added
- Firmware interface summary: `docs/FIRMWARE_INTERFACE.md`
- J2 (5V_IN) power input connector
- R3/R4 4.7 kΩ I²C pull-ups on SDA/SCL
- C5 1 µF on SGP40 VDDH
- Firmware skeleton: PlatformIO + ESP-IDF project, board pin map, shared I2C bus component with boot-time device scan
- CI workflow: clang-format check and firmware build
- CONTRIBUTING.md with branch and commit conventions, EditorConfig

### Changed
- SGP40 symbol reworked to the real 6-pin pinout (VDDH to +3V3, pin 4 to GND); reference is now U5
- Sensors are bare chips assembled by JLCPCB; PMS5003 and OLED connect via connectors
- C2 decided as 22 µF tantalum (AMS1117 output capacitor requirement)
- J1 pin 4 is No Connect
- README and Handover.txt synced with the schematic (SCD40 U3, PMS5003 U4, SGP40 U5)

### In Progress
- Footprint assignment: all parts assigned except U5 (SGP40, JLCPCB C2874215, footprint to import)
- Set the C2 value field to 22uF in the schematic

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
