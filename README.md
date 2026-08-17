# aslconverter

A wearable glove that reads hand shape and finger contact to recognize ASL fingerspelling letters, built on an ESP32-S3 with an on-device TinyML classifier.

## Overview

The glove combines three sensor types to distinguish letters that look identical on flex data alone:

- **5 flex sensors** (one per finger) measure how curled each finger is
- **4 touch sensors** (between adjacent fingers) detect direct finger-to-finger contact — needed for letters like F, O, T, and S, which have near-identical flex readings but differ in which fingers touch
- **1 IMU (MPU6050)** measures hand orientation via accelerometer, used to disambiguate handshape pairs that differ mainly by rotation (e.g. K vs. P, G vs. Q, H vs. U)

Sensor data streams over serial into a custom Edge Impulse model, which classifies static letter handshapes for on-device inference on the ESP32-S3.

## Repository structure

```
firmware/
  firmware.ino          Arduino sketch — sensor reading, calibration, serial output

hardware/
  pcb/kicad/
    asl_glove_pcb.kicad_sch    Schematic
    asl_glove_pcb.kicad_pcb    PCB layout
    asl_glove_pcb.kicad_pro    KiCad project file
    gerbers/                   Fabrication outputs (Gerbers, drill files, job file)
```

## Hardware

### PCB
2-layer board (FR-4, HASL finish, 1oz copper) designed in KiCad, hosting all sensor connectors and a socket header pair for an ESP32-S3 DevKit (Freenove ESP32-S3-WROOM). Fabricated via JLCPCB.

### Flex sensors
| Finger | GPIO |
|---|---|
| Thumb | 1 |
| Index | 2 |
| Middle | 4 |
| Ring | 5 |
| Pinky | 6 |

Each sensor forms a voltage divider with a 10kΩ resistor to GND; the midpoint feeds an ADC1-capable GPIO.

### Touch sensors
| Sensor | GPIO |
|---|---|
| Thumb–Index (T-I) | 7 |
| Index–Middle (I-M) | 15 |
| Middle–Ring (M-R) | 16 |
| Ring–Pinky (R-P) | 17 |

Digital inputs using the ESP32's internal pull-up (`INPUT_PULLUP`) — no external resistors required. Reads LOW when fingers touch (shorted to GND), HIGH when apart.

### IMU
MPU6050 over I2C — SDA on GPIO8, SCL on GPIO9 (ESP32-S3 default I2C pins).

## Firmware (`firmware/firmware.ino`)

- **Calibration on boot** — straight/bent reference per flex sensor, then a neutral-position accelerometer offset, both prompted interactively over serial
- **EMA filtering** on flex readings (α = 0.15) to smooth sensor noise
- **Bad-reading rejection** — a raw ADC value of 0 is treated as a glitch and skipped, holding the last valid filtered reading instead of reporting a false zero
- **CSV streaming** over serial at ~32Hz, one line per sample:

```
thumb,index,middle,ring,pinky,ti,im,mr,rp,accelX,accelY,accelZ,gyroX,gyroY,gyroZ
```

## Machine learning pipeline

Model built and trained in Edge Impulse:

- **Data collection:** `edge-impulse-data-forwarder` streams the serial CSV output into labeled training samples, one short recording per letter
- **Input features:** 12 of the 15 streamed values (flex ×5, touch ×4, accel ×3) — gyro is excluded from the static-letter model, since it only carries signal during motion, not held poses
- **Processing block:** Flatten (mean/min/max/RMS per axis), with feature normalization (StandardScaler) enabled to put flex (0–100), touch (0/1), and accel (small decimals) on comparable scales
- **Learning block:** Classification (Keras neural network)
- **Deployment:** exported as an Arduino library for fully on-device inference — no network connection required at runtime

### Scope

Targets the **24 static fingerspelling letters** (A–Z excluding J and Z). J and Z are motion-based letters — traced in the air rather than held — and are planned as a separate detection path using gyroscope motion-burst thresholds rather than static classification.

## Known issues / in progress

- **Flex sensor drift within a session** — sensors can shift slightly inside their glove mounting over time, causing readings for the same handshape to gradually change during a long recording session. Mitigation in progress: securing sensor mounting points and/or periodic in-session recalibration.
- Full 24-letter training data collection is in progress.

## Status

🚧 Active development — hardware fabricated (5 flex + 4 touch + IMU), firmware stable, ML pipeline validated end-to-end on a 2-letter test set. Full alphabet data collection and training in progress.
