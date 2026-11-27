# Power budget (AMS1117-3.3)

Validation of the 5 V -> 3.3 V rail, as required before PCB fabrication.
Status: **calculated from datasheets, not yet measured on hardware.**

## Verdict

- **Fine** for the current firmware (no Wi-Fi) and for typical Wi-Fi use.
- **Not safe** as drawn for the stacked worst case (Wi-Fi TX peaks plus the
  SCD40 peak current) on the default ~90 C/W footprint: the junction would
  exceed 125 C. It is fine in that case only with a large copper pour
  (~46 C/W) or a switching regulator.
- Cheapest fix: give U7 a large copper pour under/around its tab and keep the
  5 V input at or above ~4.75 V at the board.

## Loads on the 3.3 V rail

| Device | Typical / avg | Worst case | Source |
|---|---|---|---|
| ESP32-WROOM-32 | 80 mA | 500 mA | Module datasheet: avg 80 mA; supply must deliver >= 500 mA |
| SCD40 | 15 mA | 205 mA peak (18 mA avg max) | SCD4x datasheet, Table 4 (3.3 V) |
| SGP40 | 2.6 mA | 3.0 mA | SGP40 datasheet, Table 2 |
| BME280 | 0.4 mA | 1 mA | Estimate, verify |
| SSD1306 module | 20 mA | 25 mA | Estimate, module-dependent, verify |

The PMS5003 runs from the 5 V rail directly, not through U7 (~100 mA
working per its manual, not re-verified here; check fan start-up current).

## Results (Vin = 5.0 V, ground current taken as the 10 mA maximum)

P = (Vin - Vout) x I + Vin x Iq. Rise = P x theta_JA. Limit: Tj must stay
below 125 C (thermal shutdown at 165 C).

| Scenario | Load | P | Tj at 25 C ambient (90 / 46 C/W) | Tj at 50 C ambient (90 / 46 C/W) |
|---|---|---|---|---|
| A typical, no Wi-Fi | 118 mA | 0.25 W | 48 / 37 C | 73 / 62 C |
| B worst, no Wi-Fi | 314 mA | 0.58 W | 78 / 52 C | 103 / 77 C |
| C worst, Wi-Fi peak | 734 mA | 1.30 W | **142** / 85 C | **167** / 110 C |

Scenario C stacks every maximum at once, which is pessimistic: Wi-Fi TX
bursts last milliseconds and are not synchronised with the SCD40 peak. Average
dissipation with Wi-Fi in normal use is far lower, but the 734 mA peak still
sits close to the AMS1117's 800 mA rating (current limit 900 mA min).

## Dropout

Dropout is 1.1 V typical, 1.3 V max at 800 mA. With 5.0 V nominal input there
is 1.7 V of headroom; at 4.75 V there is 1.45 V. Cable and connector (J2)
drops at high current eat into this, so measure 5 V at the board under load.

## Input supply

The 5 V source must supply the 3.3 V rail current (linear regulator) plus the
PMS5003: roughly 220 mA typical, up to ~0.85 A worst case. Use a >= 1 A supply.

## Open items

- Measure real current on the first board (ESP32 with and without Wi-Fi).
- Check output droop during Wi-Fi bursts with a scope; consider extra bulk
  capacitance near U1 if the 3.3 V rail dips.
- If Wi-Fi telemetry is planned, consider a switching regulator instead of U7.

## Sources

- Sensirion SCD4x datasheet; SGP40 datasheet
- Espressif ESP32-WROOM-32 datasheet (Tables 1, 5)
- AMS1117 datasheet (dropout, ground current, theta_JA 90 C/W, Tj limits)
