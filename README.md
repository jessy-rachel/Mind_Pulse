# MindPulse — Context-Aware Classroom Environment Monitor

**MindPulse** is an Arduino prototype designed to monitor classroom environmental conditions that may be associated with discomfort and reduced concentration. The supplied sketch combines sensor signals into **heuristic** indices and controls local indicators and actuators. These indices are prototype engineering measures, **not clinically validated measures of stress, cognitive load, or learning performance**.

![MindPulse physical classroom prototype](/prototype.jpg)

*Photograph of the assembled cardboard classroom prototype with Arduino, breadboards, and LCD.*

## Features present in the supplied sketch

- **Three selectable contexts:** `LECTURE`, `GROUP`, and `EXAM`, cycled using a push button.
- Environmental sensing using a **DHT22**, an analog **LDR**, an analog **sound sensor**, a **PIR motion sensor**, and an **MQ-135** analog gas-sensor signal.
- A **Stress Index (SI)** and **Cognitive Load Index (CLI)** based on weighted environmental risk estimates.
- Startup calibration and slowly adapting noise and air-quality baselines.
- Simple trend-based risk prediction and serial debugging output.
- 16×2 I²C LCD feedback, green/yellow/red LEDs, buzzer, an LED for poor lighting, and a relay output to control a fan.

## Hardware / pin map

| Component | Pin |
|---|---|
| DHT22 data | D2 |
| PIR motion input | D3 |
| Green / yellow / red LEDs | D4 / D5 / D6 |
| Mode push button | D7 |
| Buzzer | D8 |
| Fan relay control | D9 |
| Auxiliary lighting LED | D10 |
| LDR | A0 |
| Analog sound sensor | A1 |
| MQ-135 analog output | A2 |
| I²C 16×2 LCD | Address 0x27; **SDA A4**, **SCL A5** on Uno |

**Electrical safety:** Never power a motor/fan directly from an Arduino output. Use a suitably rated relay or driver, separate external power as required, and appropriate flyback protection. Do not experiment with mains wiring without qualified supervision.

## How the prototype works



The code comments describe the weighted estimates as:

- `SI = 0.60 × noise risk + 0.25 × comfort risk + 0.15 × light risk`
- `CLI = 0.60 × air risk + 0.25 × comfort risk + 0.15 × noise risk`

**These formulas are prototype heuristics**, not established psychological or educational assessment instruments. Sensor thresholds marked `TUNE` in the sketch must be calibrated for the actual hardware and classroom conditions.

## Getting started

1. Install Arduino IDE and select the compatible Arduino board.
2. Install a compatible **LiquidCrystal_I2C** library and the **Adafruit DHT sensor library** (and dependencies).
3. Wire the sensors and outputs according to the pin map; confirm relay power and electrical ratings.
4. Open [`/MindPulse(1).ino`](/MindPulse(1).ino) and verify the LCD address and tuning constants.
5. Upload, allow startup baseline calibration in normal room conditions, and view Serial Monitor for diagnostics.
6. Press the mode button to cycle between lecture, group and exam contexts.

## Original design and connection references

- [Illustrated Arduino wiring reference (PDF)](docs/MindPulse_Arduino_Wiring.pdf) — all sensors, LEDs, button, LCD, buzzer and fan relay.
- [MindPulse concept and proposed architecture (PDF)](/MindPulse_Project_Concept.pdf) — motivation, sensing/cyber/actuation layers, mode selection and proposed risk-index approach.

The proposal describes **intended system capabilities** and includes an explicitly labelled AI-generated concept image. The photograph above is the actual prototype. Proposed features (including any NeoPixel strip or predictive performance) should not be taken as verified unless they are present in the sketch and demonstrated.

## Project status and limitations

This repository preserves the supplied project firmware. Real-world measurements, predictive performance, and any effect on concentration or learning outcomes have **not been independently validated** here. Gas sensors such as MQ-135 cannot directly provide a reliable CO₂ concentration or indoor-air-quality diagnosis without appropriate characterization and calibration.

## Files

- `/MindPulse(1).ino` — original Arduino firmware
- `/prototype.jpg` — physical prototype photo
- `/MindPulse_Arduino_Wiring.pdf` — illustrated wiring guide
- `/MindPulse_Project_Concept.pdf` — original concept and architecture
- `README.md` and `.gitignore`
