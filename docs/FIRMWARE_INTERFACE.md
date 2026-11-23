# Firmware Interface Summary — Environment_Sensor (ESP32-WROOM-32)

Verified against `hardware/schematic/Environment_Sensor.kicad_sch` at commit `fdfb6cb` (netlist traced from the
file). Items marked **unverified** come from datasheet recall or module-dependent defaults. If the schematic
changes, update this file in the same commit.

## 1. ESP32 pins in use

| GPIO | Module pin | Net / function | Connected part and pin |
|---|---|---|---|
| EN (not a GPIO) | 3 | Reset | R1 10k to +3V3; SW2 (RESET) to GND |
| GPIO0 | 25 | Boot strap | R2 10k to +3V3; SW1 (BOOT) to GND |
| GPIO1 (U0TXD) | 35 | Serial TX | J1 pin 3 (adapter RX) |
| GPIO3 (U0RXD) | 34 | Serial RX | J1 pin 2 (adapter TX) |
| GPIO16 | 27 | UART2 RX (PMS_RX) | U4 PMS5003 pin 5 (TX) |
| GPIO17 | 28 | UART2 TX (PMS_TX) | U4 PMS5003 pin 4 (RX) |
| GPIO21 | 33 | I2C SDA | U2 SDI, U3 SDA (pin 10), U5 SDA (pin 3), U6 SDA (pin 4), R3 pull-up |
| GPIO22 | 36 | I2C SCL | U2 SCK, U3 SCL (pin 9), U5 SCL (pin 6), U6 SCL (pin 3), R4 pull-up |

Flagged No Connect (floating): GPIO2, 4, 5, 12, 13, 14, 15, 18, 19, 23, 25, 26, 27, 32, 33, 34, 35,
SENSOR_VP (36), SENSOR_VN (39). GPIO6–11 are the module's internal flash and are not usable.

Net names: `I2C_SDA`, `I2C_SCL`, `PMS_RX` and `PMS_TX` are used in the docs as signal names only. The schematic
has no such net labels; its only label is a stray `SGP40` on the SDA net. Add real labels before PCB work.

Firmware notes:
- No DTR/RTS auto-reset on J1: to flash, hold BOOT (SW1), tap RESET (SW2), release BOOT.
- `Wire.begin(21, 22)`; PMS5003 on `Serial2` at 9600 8N1 (RX=16, TX=17). Console/flash is UART0 on GPIO1/3.

## 2. I2C devices (3.3 V bus)

| Ref | Part | Address | Note |
|---|---|---|---|
| U2 | BME280 | 0x76 | SDO to GND, CSB to +3V3 (I2C mode) |
| U3 | SCD40-D-R2 | 0x62 | **Unverified**: from memory of the Sensirion SCD4x datasheet |
| U5 | SGP40 | 0x59 | Sensirion datasheet v1.2 |
| U6 | SSD1306 OLED module | 0x3C typical | **Unverified**: depends on the module (0x3D if its address pin is high) |

Pull-ups: R3 = 4.7 kΩ (SDA to +3V3), R4 = 4.7 kΩ (SCL to +3V3), both on the board. No other pull-ups in the
schematic. If the OLED module has its own, they add in parallel.

## 3. PMS5003 (U4) SET / RESET

- SET (pin 3) and RESET (pin 6) are both left unconnected, with No Connect flags.
- Plantower manual: SET high or floating = normal operation, low = sleep; RESET low = reset.
- Consequence: firmware cannot sleep or reset the PMS5003 (no GPIO wired). Only a 5 V power cycle resets it.
  If sleep control is wanted, a spare GPIO (e.g. GPIO25) would have to be wired to SET: a schematic change.
- Powered from +5V (VCC pin 1); its UART is 3.3 V logic, wired directly to the ESP32.

## 4. Final reference designators

| Ref | Part | Value / note |
|---|---|---|
| U1 | ESP32-WROOM-32 | module |
| U2 | BME280 | bare chip |
| U3 | SCD40-D-R2 | bare chip (Handover.txt originally said U4) |
| U4 | PMS5003 | connector (8-pin 1.25 mm), uses the sensor's cable |
| U5 | SGP40 | bare chip (value field empty in the schematic) |
| U6 | SSD1306 OLED | module on a 4-pin header (value field empty in the schematic) |
| U7 | AMS1117-3.3 | |
| C1 | 10µF | +5V input |
| C2 | 22µF tantalum (decided) | +3V3 output; the schematic value field still reads `C_Polarized`, so set it to 22uF |
| C3 / C4 | 100nF / 10µF | ESP32 +3V3 decoupling |
| C5 | 1µF | SGP40 VDDH |
| R1 / R2 | 10k / 10k | EN / GPIO0 pull-ups |
| R3 / R4 | 4.7k / 4.7k | SDA / SCL pull-ups |
| SW1 / SW2 | tact switches | BOOT / RESET |
| J1 | 4-pin header | UART_PROGRAMMING |
| J2 | 2-pin header | 5V_IN (pin 1 +5V, pin 2 GND) |

## 5. Decisions made since the README / Handover were written

1. SGP40 symbol rebuilt to the real 6-pin pinout: pin 4 to GND, pin 5 (VDDH) to +3V3, pin 6 SCL; 1µF C5 on VDDH; ref now U5.
   The old 4-pin symbol had SCL on a pin that must be grounded.
2. I2C pull-ups added (R3, R4 at 4.7 kΩ). The doc listed none.
3. J1 pin 4 changed from a wire to +3V3 to No Connect, matching the doc's own rule against feeding board power from the adapter.
4. J2 added as the 5 V power input; the doc had no input connector.
5. C2 changed from 10µF to 22µF tantalum, per the AMS1117 datasheet's output-capacitor requirement; C1 is 10µF.
6. BME280 SDO: stray No Connect flag removed (SDO wired to GND); resulting ERC pin_to_pin warning is excluded as harmless.
7. PMS5003 SET/RESET left floating, confirmed acceptable by the Plantower manual.
8. Sensors (BME280, SCD40, SGP40) are bare chips assembled by JLCPCB. PMS5003 and OLED connect by connector/header.
9. Footprints assigned in the schematic: passives `C_0805`/`R_0805`; C2 `CP_EIA-3528-21_Kemet-B`; SW1/SW2 `SW_SPST_PTS645Sx43SMTR92`
   (verify against the JLCPCB switch); J1 `PinHeader_1x04`; J2 `PinHeader_1x02`; U1 `ESP32-WROOM-32`; U2 `Bosch_LGA-8_2.5x2.5mm`;
   U3 `Sensirion_SCD4x-1EP_10.1x10.1mm`; U4 `Molex_PicoBlade_53398-0871_1x08`; U6 `FanPinHeader_1x04_P2.54mm_Vertical`; U7 `SOT-223-3_TabPin2`.
   U5 has no footprint yet: JLCPCB part C2874215 (DFN-6-EP 2.4x2.4), footprint to be imported.

## 6. Still open

- AMS1117 power and thermal budget (ESP32 Wi-Fi peaks) not yet calculated.
- SCD40 and OLED addresses unverified; JLCPCB stock for C2874215 unconfirmed.
- SCD40 unused pins are unreviewed (SCD40 pins 7/19 VDD and 6/20/21 GND are wired; nothing else).
