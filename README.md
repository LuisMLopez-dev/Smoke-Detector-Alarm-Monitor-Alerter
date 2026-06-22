# Smoke-Detector-Alarm-Monitor-Alerter
The program files and firmware for a senior design project on omproving the accessibility for smoke detector alerts are contained here.

## Overview
Smoke detectors rely primarily on adubile alerts which can ineffective for individuals who are deaf, hard of hearing, or have varying degrees of hearing loss. The goal of this project is to develop a low-cost, non-invasive, no installation-required alert system that detects smoke alarm patterns and wirelessly communicates to multiple receiver alarm units.

## Main Components
1. Listener Unit
2. Placeable Alarm Unit
3. Wearable Alarm Unit
4. Smartphone Application

### Listener Unit
- Digital signal processing on an audio input signal
- Detects the Temporal-3 (T3) alarm pattern which is three beeps, each 0.5 s, and then a pause that is 1.5 s
- Uses ADC sampling and timing-based state transitions for pattern recognition

Transmits alert status via:
1. ESP-NOW which is device-to-device communication
2. Bluetooth Low Energy, or BLE, for smartphone connectivity

### Placeable Alarm Unit
- Receives ESP-NOW messages/data from the listener unit
- Activates high-brightness LEDs for clear visual alerts
- Designed for placement in important locations such as the bedroom, living room, etc.

### Wearable Alarm Unit
- Receives ESP-NOW messages/data from the listener unit
- Activates a vibration motor to alert the user through haptic feedback
- Designed to provide immediate personal alerts

### Smartphone Application: WIP!
- Connects to the listener unit via BLE
- Receives alarm notifications in real time
- Utilizes vibration and high-priority notifications to alert the user
