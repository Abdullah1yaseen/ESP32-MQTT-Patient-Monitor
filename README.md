# ESP32-MQTT-Patient-Monitor

Offline IoT patient monitoring system built on two ESP32 units communicating over a local WiFi network via MQTT — no internet or external router required. A sender unit reads vital signs from the patient, and a receiver unit hosts the network, broker, and display. 🩺📡

## Requirements

**Hardware:**

* 2x ESP32 Dev Board (one sender, one receiver)
* MAX30102 Pulse Oximeter Sensor (heart rate + SpO2)
* MLX90614 Infrared Temperature Sensor (body + ambient temp)
* SH1106 OLED Display (I2C) — sender status screen
* ILI9341 TFT Display (SPI) — receiver monitor screen
* 5x 3.7V Lithium-ion batteries + power switch + boost converter (step-up)

**Software:**

* Arduino IDE

**Arduino Libraries:**

* WiFi.h (built-in)
* Wire.h (built-in)
* SPI.h (built-in)
* PubSubClient
* PicoMQTT
* ArduinoJson
* Adafruit GFX Library
* Adafruit SH110X
* Adafruit MLX90614 Library
* SparkFun MAX3010x Pulse and Proximity Sensor Library (MAX30105.h)
* Ucglib
* [spo2_algorithm.h](https://github.com/Abdullah1yaseen/ESP32-MQTT-Patient-Monitor/blob/main/heartRateProject/spo2_algorithm.h) — not available via Library Manager, must be downloaded manually and placed inside the `heartRateProject` folder

## Installation

1. Clone the project

   ```
   git clone https://github.com/Abdullah1yaseen/ESP32-MQTT-Patient-Monitor.git
   ```
2. Download `spo2_algorithm.h` from the link above and place it inside the `heartRateProject` folder
3. Install all required Arduino libraries listed above
4. Upload `heartRateProjectRec` to the receiver ESP32 first (creates the WiFi AP + MQTT broker)
5. Upload `heartRateProject` to the sender ESP32 (connects to the receiver and streams sensor data)
6. Power both units and place a finger on the MAX30102 sensor to start a reading

## Project Structure

```
ESP32-MQTT-Patient-Monitor/
├── heartRateProject/                          # Sender firmware (patient side)
│   ├── heartRateProject.ino
│   └── spo2_algorithm.h      # downloaded manually, see Installation
├── heartRateProjectRec/
│   └── heartRateProjectRec/                   # Receiver firmware (monitor side)
│       └── heartRateProjectRec.ino
├── 3dDesign/                                  # 3D-printable enclosure design files
└── README.md
```

## Wiring

**Sender unit:**
| Component | ESP32 Pin |
|---|---|
| MAX30102 (SDA/SCL) | GPIO 21 / GPIO 22 |
| MLX90614 (SDA/SCL) | Shared I2C bus |
| SH1106 OLED (SDA/SCL) | Shared I2C bus |
| Battery pack | 5V via boost converter |

**Receiver unit:**
| Component | ESP32 Pin |
|---|---|
| ILI9341 TFT — DC | GPIO 2 |
| ILI9341 TFT — CS | GPIO 15 |
| ILI9341 TFT — RST | GPIO 4 |

## How It Works

1. The sender unit continuously checks the MAX30102 sensor for a finger placement using an IR threshold.
2. Once detected, it takes several consecutive readings, discards out-of-range values, and averages the rest for a more stable heart rate and SpO2 result.
3. Body and ambient temperature are read from the MLX90614 sensor and combined with the heart rate/SpO2 readings into a single JSON payload.
4. The sender connects to the receiver's local WiFi access point and publishes the JSON payload to the `patientData` MQTT topic.
5. The receiver — which runs its own WiFi AP and local MQTT broker (no internet needed) — receives the data and displays it on the TFT screen.
6. The receiver checks each value against safe thresholds:
   * SpO2 below 92% → `LOW SPO2!` alert
   * Heart rate above 120 or below 50 BPM → `ABNORMAL HR!` alert
   * Temperature outside 35–38°C → `HIGH TEMP!` alert
7. If no data is received for 30 seconds, the receiver switches to a standby screen with an animated heart icon and a simulated ECG waveform.
8. After each reading, the sender counts down 30 seconds before the next measurement; this timer can be reset remotely via a `RESET_TIMER` command on the `esp32/unit01/cmd` topic.

## Power Supply

The system runs fully off-grid using a **5x 3.7V lithium-ion battery pack**, a **power switch**, and a **boost converter (step-up)** to regulate the voltage output to a stable level for the ESP32 boards and sensors.

## Notes

* Fully offline system — no internet connection or external router is used; all communication happens over the local WiFi network created by the receiver.
* The `3dDesign` folder contains the 3D-printable enclosure files for both units.
* This project is a prototype for educational purposes and is not a certified medical device.
