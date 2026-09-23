# esp-ePaper-bin-tracker

A firmware project for an ESP32-C6 paired with a 1.54-inch Waveshare ePaper display for a smart bin tracker.

## Development environment

This repo is set up for PlatformIO with the Espressif32 platform and a board target for the ESP32-C6.

### Prerequisites
- Python 3.10+
- PlatformIO Core (`pip install platformio`)
- USB serial driver for the ESP32-C6 board

### Quick start
1. Install PlatformIO.
2. From the repository root run:
   - `pio run`
   - `pio run -t upload`
   - `pio device monitor`

### Board assumptions
- MCU: ESP32-C6
- Display: 1.54-inch ePaper panel
- Framework: Arduino
- Preferred libraries: `GxEPD2` and related display helpers

### Repo conventions
- `.agent.md` defines a custom agent for firmware work on this hardware.
- `.gitignore` excludes build output, IDE files, and generated firmware artifacts.
- `.gitattributes` keeps line endings consistent while treating binary assets as binary.
