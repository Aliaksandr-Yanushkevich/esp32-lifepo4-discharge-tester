ESP32-C3 LiFePO4 Discharge Tester

Minimal battery discharge tester for single LiFePO4 cells based on ESP32-C3 Mini.

🚀 Features
ESP32-C3 Mini control
Voltage measurement via ADC
MOSFET-controlled load
Automatic cutoff by voltage
Hysteresis (no oscillation)
Telegram bot control (buttons)
No display required
Low-cost components
Calibrated voltage readings
No message spam
🔧 Hardware
Components
Component	Description
ESP32-C3 Mini	Main controller
IRLB8743	N-channel MOSFET
Resistors	Load
100k / 100k	Voltage divider
0.1 µF	ADC filter capacitor
⚡ Electrical Connections
Load path
BAT+ → Load → MOSFET (Drain)
MOSFET (Source) → GND
BAT- → GND
Voltage measurement
BAT+ ---- R1 ----+---- R2 ---- GND
                 |
               GPIO0 (ADC)
                 |
               0.1µF
                 |
                GND
MOSFET control
GPIO5 ---- 100Ω ---- Gate
Gate  ---- 100k ---- GND
Source ---- GND
Drain  ---- Load
📌 Pinout
Pin	Function
GPIO0	ADC (voltage)
GPIO5	MOSFET gate
⚙️ Firmware Behavior
Continuous voltage measurement
Calibration applied to readings
MOSFET controlled by voltage threshold
Hysteresis prevents oscillation
Telegram used as UI
Messages only on events (no spam)
📡 Telegram Interface
Main buttons
▶ Start
⏹ Stop
📊 Status
🧪 Diag
⚙ Cutoff
❓ Help
Cutoff selection
2.80 V
2.90 V
3.00 V
3.05 V
3.10 V
3.20 V
3.30 V
🎯 Calibration

Formula:

V_real = A * V_measured + B

Current calibration:

A = 0.8938
B = 0.0000

Calibration is based on multimeter comparison.

🔋 Recommended Cutoff
Voltage	Usage
3.00 V	✅ Recommended
2.80 V	⚠ Maximum capacity
2.00 V	❌ Absolute minimum only
🧪 Serial Diagnostics

Baud rate: 115200

Output includes:

ADC raw
ADC voltage
Raw battery voltage
Calibrated voltage
Cutoff
MOSFET state
Calibration values
🧠 Design Philosophy
Keep hardware simple
Avoid unnecessary modules
Use software instead of hardware UI
Focus on reliability
Build iteratively
⚠️ Limitations
No current measurement
No Ah calculation
No temperature monitoring
No logging
Single-cell only
🚧 Future Improvements
Current measurement (shunt)
Ah counter
Auto calibration
Temperature monitoring
Logging
Internal resistance estimation
⚠️ Safety
Use proper wire thickness
Ensure resistor cooling
Add fuse if possible
Avoid deep discharge
Use common ground

Do not rely on this as the only protection system.

📌 Status

Current state:

Working prototype
Stable measurements
Telegram control
Calibrated readings
Safe cutoff logic
📄 License
