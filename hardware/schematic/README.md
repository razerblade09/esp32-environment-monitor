# Schematic

This directory contains the KiCad schematic project for the ESP32 Environmental Monitor.

## Files

| File | Description |
|---|---|
| `Environment_Sensor.kicad_pro` | KiCad project file |
| `Environment_Sensor.kicad_sch` | Schematic source |
| `Environment_Sensor.kicad_pcb` | PCB file (empty — layout not started) |
| `ERC.rpt` | Electrical Rules Check report |

## ERC Status

**Result:** 0 errors · 1 known benign warning  
See [Known Issues](../../README.md#known-issues--notes) in the main README for details on the BME280 SDO / PWR_FLAG pin-type mismatch warning.

## Opening in KiCad

Open `Environment_Sensor.kicad_pro` with **KiCad 10** (the version used to create it).
