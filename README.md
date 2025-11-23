🗣️ Embedded Voice Command Recognition System
ESP32 + MAX9814 / INMP441 + EdgeImpulse + Google STT + MQTT + IoT Sensors

This project implements a two-phase voice-controlled embedded system built on ESP32, capable of:

1. Offline keyword recognition (EdgeImpulse)
2. Online cloud-based speech-to-text (Google Speech-to-Text API)
3. Real-time IoT sensor monitoring (DHT11)
4. Cloud upload using MQTT → ThingSpeak
5. Voice-controlled commands for automation

📌 Project Phases

✔ Phase 1 — Offline Voice Command Recognition (EdgeImpulse Model)

Hardware: ESP32 + MAX9814 (ADC based)
This phase uses on-device ML inference with an EdgeImpulse neural network, recognizing:
"yes" → LED ON
"no" → LED OFF
"marvin" → LED blink

Core features:

1. 16 kHz ADC audio capture
2. Normalization + soft AGC
3. Real-time inference using EdgeImpulse C++ SDK
4. Confirmation logic to avoid false triggers
5. Runs 100% offline.
6. This phase demonstrates embedded keyword spotting without internet — fast, lightweight, and reliable.






✔ Phase 2 — Cloud STT + IoT Control + MQTT Uploading

Hardware: ESP32 + INMP441 (I2S), DHT11 sensor, WiFi, Google STT

Phase 2 is a major upgrade:
Voice commands are converted into text using Google Speech-to-Text, and then used to control IoT operations.

Supported voice commands:

1."start / on / activate" → Start auto sensor upload every 20 sec

2."stop / off / deactivate" → Stop monitoring

3."read / show / temperature / humidity" → Show sensor data

4."upload / send / publish" → Upload to cloud immediately

5."help" → Display all available commands

Cloud upload uses MQTT → ThingSpeak for live dashboards.


Additional features:


1.3-second high-quality audio recording (16-bit PCM / I2S)

2.Base64 streaming to Google STT API

3.Automatic DHT11 reading + printing

4.Automatic cloud upload timer

5.Proper DC offset calibration + AGC
