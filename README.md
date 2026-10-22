# ESP32-C3 LiFePO4 Discharge Tester

[English](README.md) | [Polski](README.pl.md)

A prototype discharge tester for a single LiFePO4 cell, using an ESP32-C3 Mini, a resistor load, and a MOSFET. Telegram provides the controls; no display is required.

The firmware is in [`lifepo4_tester.ino`](lifepo4_tester.ino). It measures cell voltage and switches the load at a selectable cutoff. It does **not** measure current or calculate capacity (Ah/Wh).

Photos: [shared iCloud album](https://www.icloud.com/sharedalbum/#D1gv3kj9t14uM_-qoxNOJfSF_bACAEQARogxGbpijmXgZkVoKax6umBtObMlxxSzGx67AQJz0zXkXI).

## Hardware and wiring

| Component | Value / purpose |
| --- | --- |
| ESP32-C3 Mini | Controller |
| IRLB8743 N-channel MOSFET | Switches the resistor load |
| Load resistor(s) | Select resistance and power rating for the intended discharge load |
| Voltage divider | Two 100 kΩ resistors |
| ADC filter capacitor | 0.1 µF between ADC input and ground |
| Gate series resistor | 100 Ω |
| Gate pulldown resistor | 100 kΩ |

| ESP32 pin | Connection |
| --- | --- |
| GPIO0 | Voltage-divider midpoint (ADC) |
| GPIO5 | MOSFET gate through 100 Ω |
| GND | Cell negative and MOSFET source |

```text
Load path:

BAT+ ---- Load resistor(s) ---- Drain (MOSFET)
                               Source ---- GND ---- BAT-

Voltage measurement:

BAT+ ---- 100 kΩ ----+---- 100 kΩ ---- GND
                    |
                  GPIO0
                    |
                  0.1 µF
                    |
                   GND

Gate control:

GPIO5 ---- 100 Ω ---- Gate
                     |
                   100 kΩ
                     |
                    GND
```

The divider has a nominal factor of 2. All grounds must be connected together. The diagram shows the measurement and load connections; it does not specify an ESP32 power supply. Use a supply appropriate for your particular board.

## Firmware setup

1. Install the ESP32 board support package in Arduino IDE and select the board/port matching your ESP32-C3 hardware.
2. Install the **UniversalTelegramBot** and **ArduinoJson** libraries. `WiFi`, `WiFiClientSecure`, and `Preferences` are supplied by the ESP32 board package.
3. Put `lifepo4_tester.ino` in a sketch folder named `lifepo4_tester` and open it in Arduino IDE.
4. Replace the placeholders at the top of the sketch:

   | Setting | Value |
   | --- | --- |
   | `WIFI_SSID` | Wi-Fi network name |
   | `WIFI_PASSWORD` | Wi-Fi password |
   | `BOT_TOKEN` | Telegram bot token |
   | `CHAT_ID` | Authorized Telegram chat ID, as a string |

5. Check the wiring and voltage calibration before using the load, then upload the sketch.
6. Open Serial Monitor at **115200 baud** for connection messages and diagnostics.

Keep real credentials out of commits. The repository does not pin board-package or library versions.

## Discharge behavior

| Setting | Default |
| --- | --- |
| Cutoff | 3.00 V, unless a saved value exists |
| Recovery hysteresis | 0.05 V |
| Control interval | 500 ms, when the loop is not blocked |
| Telegram polling interval | 1,200 ms, when the loop is not blocked |
| Serial diagnostics interval | 5 seconds, when the loop is not blocked |

- On boot, the MOSFET output is set LOW and discharge is disabled. A previous discharge session is not resumed.
- **Start** enables discharge and clears the cutoff latch. The next control pass turns on the MOSFET without checking that voltage is above cutoff first.
- While the MOSFET is on, a reading at or below cutoff turns it off and sets the cutoff latch.
- Discharge stays enabled after cutoff. If voltage rises **above cutoff + 0.05 V**, the load turns on automatically. Cell-voltage recovery can therefore cause repeated cutoff/resume cycles.
- **Stop** disables discharge and switches the MOSFET off when the command is processed.
- Selecting a cutoff saves it to ESP32 nonvolatile storage (`tester` namespace, `cutoff` key). It takes effect on subsequent control passes and survives reboot. Calibration and hysteresis are defined in the sketch.

Despite the state name `CUTOFF_LATCHED`, cutoff is not a permanent stop requiring a new Start command.

## Telegram controls

Only messages from the configured `CHAT_ID` are accepted. This is a chat-level check; it does not restrict individual senders within an authorized group chat.

| Button | Command | Action |
| --- | --- | --- |
| ▶ Start | `/start` | Enable discharge and clear the cutoff latch |
| ⏹ Stop | `/stop` | Disable discharge and turn off the load |
| 📊 Status | `/status` | Show voltage, cutoff, state, and MOSFET/latch flags |
| 🧪 Diag | `/diag` | Show ADC readings, calibration, and control flags |
| ⚙ Cutoff | Button only | Open cutoff selection |
| ❓ Help | `/help` | Show controls and calibration values |
| ⬅ Back | Button only | Return to the main menu |

**Sending `/start` starts discharge**, including the `/start` message commonly used to open a new bot conversation.

Available cutoff choices: **2.80, 2.90, 3.00, 3.05, 3.10, 3.20, and 3.30 V**. Choose a cutoff appropriate for the cell and test conditions using the cell manufacturer's specifications.

The bot sends startup, command-response, cutoff, and automatic-resume messages. There are no periodic Telegram status reports; repeated cutoff/resume cycles can still generate repeated notifications.

## Voltage measurement and calibration

Each voltage reading averages 64 raw ADC samples, with a 300 µs delay between samples. The ADC is configured for 12-bit resolution and `ADC_11db` attenuation.

The sketch calculates voltage as follows:

```text
V_adc      = ADC_average / 4095 × 3.3
V_measured = V_adc × 2
V_reported = calibA × V_measured + calibB

calibA = 0.8938
calibB = 0.0000
```

The 3.3 V reference is an estimate used by the code; it is not a measured ADC reference. The included calibration constants must be checked against a multimeter on your hardware. Edit `calibA` and `calibB` in the sketch and upload again to change them. There is no calibration command in Telegram.

Serial diagnostics and Telegram **Diag** include the averaged raw ADC value, estimated ADC voltage, uncalibrated and calibrated battery voltage, cutoff, hysteresis, enabled/MOSFET/latch flags, and calibration coefficients.

## Prototype limitations

- **Network operations block discharge control.** Wi-Fi connection/reconnection retries indefinitely. During reconnection, the loop does not check voltage, and an already-on MOSFET remains on. Telegram requests and message handling can also delay control checks and Stop commands.
- **Start does not reject a cell already below cutoff.** It can switch the load on until a later control pass detects the low voltage.
- **Cutoff automatically resets after voltage recovery.** The firmware does not end the test permanently at the first cutoff.
- **TLS certificate verification is disabled** by `securedClient.setInsecure()`.
- There is no current measurement, capacity calculation, temperature monitoring, persistent measurement logging, or explicit ADC/sensor-fault detection.
- This is a single-cell design, not a pack tester or a constant-current electronic load.

Use an independent protection system; this sketch alone does not provide reliable battery protection. Size the load resistors, MOSFET cooling, and wiring for the intended load, provide suitable fusing, and verify voltage readings and switching behavior before a discharge test.

## Possible improvements

- Nonblocking network handling so voltage control continues during connection failures.
- Reject Start below cutoff and require an explicit restart after cutoff.
- Current sensing and Ah/Wh integration.
- Temperature monitoring and fault shutdown.
- Calibration controls and measurement logging.
