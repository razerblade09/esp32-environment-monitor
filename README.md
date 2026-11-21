# ESP32 Environmental Monitor

An ESP32-based custom PCB that measures and displays multiple environmental parameters — particulate matter, VOC, CO₂, temperature, humidity, and pressure — using dedicated sensors.

> **Status:** Schematic complete (ERC: 1 known benign warning) · Footprint assignment in progress · PCB layout not started

---

## Table of Contents

- [Overview](#overview)
- [Hardware](#hardware)
  - [Components](#components)
  - [Communication Architecture](#communication-architecture)
  - [Schematic](#schematic)
  - [Power Supply](#power-supply)
  - [Programming Interface](#programming-interface)
- [Project Structure](#project-structure)
- [Development Roadmap](#development-roadmap)
- [Known Issues & Notes](#known-issues--notes)
- [License](#license)

---

## Overview

This project designs a compact, custom PCB environmental monitor using the **ESP32-WROOM-32** bare module as its microcontroller. It collects sensor data and displays it on an SSD1306 OLED screen. The board is powered from a 5V external supply, regulated down to 3.3V via an AMS1117-3.3.

---

## Hardware

### Components

| Reference | Component | Purpose |
|---|---|---|
| U1 | ESP32-WROOM-32 | Main microcontroller |
| U2 | BME280 | Temperature, humidity, and pressure |
| U3 | SGP40 | VOC (Volatile Organic Compound) measurement |
| U4 | SCD40 | CO₂ measurement |
| U5 | PMS5003 | Particulate matter (PM1.0, PM2.5, PM10) |
| U6 | SSD1306 | 128×64 OLED display |
| U7 | AMS1117-3.3 | 3.3V LDO voltage regulator |
| J1 | USB-UART Header | Programming and serial communication |

---

### Communication Architecture

#### I²C Bus (GPIO21 = SDA, GPIO22 = SCL)

| Device | I²C Address | Note |
|---|---|---|
| SGP40 | 0x59 | Fixed |
| BME280 | 0x76 | SDO → GND |
| SCD40 | 0x62 | Fixed |
| SSD1306 | 0x3C | Typical module default |

#### UART2 — PMS5003 (Particulate Matter)

| ESP32 GPIO | Direction | PMS5003 Pin |
|---|---|---|
| GPIO16 (PMS_RX) | ← Receive | TX |
| GPIO17 (PMS_TX) | → Transmit | RX |

---

### Schematic

The KiCad schematic and project files are located in [`hardware/schematic/`](hardware/schematic/).

An exported PDF of the schematic is available at [`docs/images/Environment_Sensor.pdf`](docs/images/Environment_Sensor.pdf).

**Custom KiCad symbols created for this project:**
- PMS5003
- SGP40
- SSD1306 OLED (4-pin module interface)

---

### Power Supply

| Rail | Source | Regulator |
|---|---|---|
| +5V | External input | — |
| +3V3 | AMS1117-3.3 | VIN = +5V, VOUT = +3V3 |

**Decoupling capacitors:**
- C1: 10 µF — +5V rail (AMS1117 input)
- C2: 10 µF — +3V3 rail (AMS1117 output)
- C3: 100 nF — ESP32 VDD local decoupling
- C4: 10 µF — ESP32 VDD bulk decoupling

> ⚠️ **Power budget validation is pending.** The AMS1117-3.3 must be validated against total system current draw (including ESP32 Wi-Fi peaks) before PCB fabrication. See [Known Issues](#known-issues--notes).

---

### Programming Interface

A 4-pin header (J1) exposes the ESP32 UART0 for flashing firmware:

| Pin | Signal |
|---|---|
| 1 | GND |
| 2 | GPIO3 / U0RXD |
| 3 | GPIO1 / U0TXD |
| 4 | NC |

Requires a **3.3V-logic USB-UART adapter**. Do not connect the adapter's VCC to the board power rail.

---

## Project Structure

```
esp32-environment-monitor/
├── hardware/
│   ├── schematic/          # KiCad project & schematic files + ERC report
│   ├── pcb/                # PCB layout files (not started)
│   ├── gerbers/            # Manufacturing outputs (not started)
│   └── bom/                # Bill of Materials (not started)
├── docs/
│   ├── images/             # Schematic PDF and screenshots
│   └── datasheets/         # Component datasheets (add as needed)
├── firmware/               # ESP32 firmware source (not started)
├── Handover.txt            # Detailed project handover & AI context document
├── .gitignore
├── CHANGELOG.md
└── README.md
```

---

## Development Roadmap

| Phase | Status | Description |
|---|---|---|
| Schematic design | ✅ Complete | All components placed and connected; ERC run |
| Footprint assignment | 🔄 In progress | Assign KiCad footprints to each component |
| Schematic engineering review | ⏳ Pending | Verify PMS5003 SET/RESET, SCD40 unused pins, I²C pull-ups, power budget |
| PCB layout | ⏳ Pending | Board shape, component placement, routing, DRC |
| Manufacturing preparation | ⏳ Pending | Gerbers, drill files, BOM, pick-and-place |
| Firmware development | ⏳ Pending | ESP32 sensor reading and display logic |
| Hardware bring-up | ⏳ Pending | Board inspection, rail verification, sensor testing |

---

## Known Issues & Notes

### ERC Warning — BME280 SDO / PWR_FLAG Pin Type Mismatch

**Location:** `hardware/schematic/ERC.rpt`

The ERC reports 1 warning:
```
[pin_to_pin]: Pins of type Bidirectional and Power output are connected
  Symbol U2 Pin 5 [SDO, Bidirectional]
  Symbol #FLG02 Pin 1 [Power output]
```

**Assessment:** This is a **benign KiCad ERC artefact**. The BME280 SDO pin is intentionally tied to GND for I²C address selection (address 0x76). The PWR_FLAG on the GND net is required by KiCad to suppress a separate power-pin ERC error. The connection is electrically correct.

---

### PMS5003 SET and RESET Pins

The PMS5003 SET and RESET pins are currently left unconnected. Verify their required operating states against the PMS5003 datasheet before footprint assignment and PCB fabrication.

---

### SCD40 Unused Pins

Not all SCD40 pins have been reviewed. Do not assume unused pins can be left floating — confirm against the SCD40 datasheet.

---

### AMS1117-3.3 Power Budget

The regulator has not been validated for the full system load. Calculate total current consumption (especially ESP32 Wi-Fi peaks) and regulator thermal dissipation before ordering the PCB.

---

## License

This project is released under the [MIT License](LICENSE).
