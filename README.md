ESP32-C3 LiFePO4 Discharge Tester

Minimal battery discharge tester for single LiFePO4 cells based on ESP32-C3 Mini.

Features
ESP32-C3 Mini control
Voltage measurement via ADC
MOSFET controlled load
Automatic cutoff by voltage
Hysteresis control
Telegram bot with buttons
No display required
Low-cost components
Stable calibrated readings
No message spam
Hardware

Main components:

ESP32-C3 Mini
IRLB8743 MOSFET
Power resistors (load)
Voltage divider
Optional capacitor (0.1 µF)
Voltage divider

R1 = 100k
R2 = 100k

Divider ratio: 2:1

Wiring
Load path

BAT+ → Load → MOSFET Drain
MOSFET Source → GND
BAT- → GND

Voltage measurement

BAT+ → R1 → ADC → R2 → GND

ADC pin → GPIO0

Add capacitor:

ADC → 0.1µF → GND

MOSFET control

GPIO5 → 100Ω → Gate
Gate → 100k → GND
Source → GND
Drain → Load

Pinout

GPIO0 — ADC input
GPIO5 — MOSFET gate

Firmware Behavior
Measures battery voltage continuously
Applies calibration
Controls MOSFET based on cutoff
Uses hysteresis to avoid oscillation
Sends Telegram messages only on events

No periodic spam messages.

Telegram Interface

Buttons:

Start
Stop
Status
Diag
Cutoff
Help

Cutoff values:

2.80 V
2.90 V
3.00 V
3.05 V
3.10 V
3.20 V
3.30 V

Calibration

Formula:

V_real = A * V_measured + B

Current calibration:

A = 0.8938
B = 0.0000

Calibration is based on comparison with a multimeter.

Recommended Cutoff

For LiFePO4:

3.00 V — recommended
2.80 V — extended capacity (not recommended for long-term use)

2.0 V is an absolute minimum, not a working cutoff.

Serial Output

Baud rate: 115200

Diagnostics include:

ADC raw value
ADC voltage
Raw battery voltage
Calibrated voltage
Cutoff
MOSFET state
Calibration values
Project Philosophy
Keep hardware simple
Avoid unnecessary modules
Use software instead of hardware UI
Focus on reliability
Build step by step
Limitations
No current measurement
No Ah calculation
No temperature monitoring
No data logging
Single cell only
Future Improvements
Current measurement (shunt)
Ah counter
Auto calibration
Temperature monitoring
Logging
Internal resistance measurement
Safety
Use proper wire thickness
Ensure resistor cooling
Add fuse if possible
Avoid deep discharge
Always use common ground

Do not rely on this as the only battery protection system.

Status

Working prototype:

Stable measurements
Telegram control
Calibrated voltage
Safe cutoff logic
