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
2. Update the Wi-Fi SSID and password in [src/config.h](src/config.h).
3. Fill in the real Auckland address details and, once resolved, store the final address ID in the device configuration.
4. From the repository root run:
   - `pio run`
   - `pio run -t upload`
   - `pio device monitor`

### Auckland Council data source
The app is designed to call the official Auckland Council data source directly:
- https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days.html

Runtime flow:
1. Resolve the address to an ID using the address search API.
   - Example: `GET https://www.aucklandcouncil.govt.nz/api/address/search?query=500+Queen+Street`
2. Fetch the weekly collection dates for that resolved address.
   - Example: `GET https://www.aucklandcouncil.govt.nz/api/kerbside?address=12342478585`
3. Render the result like:
   - `Rubbish: Thursday, 24 September`
   - `Food scraps: Thursday, 24 September`
   - `Recycling: Thursday, 24 September`

### Board assumptions
- MCU: ESP32-C6
- Display: 1.54-inch ePaper panel
- Framework: ESP-IDF
- Preferred libraries: `GxEPD2` and related display helpers

### Required device config
The app needs two things at runtime:
- Wi-Fi credentials so the device can reach Auckland Council's APIs.
- The resolved address details plus the address ID that is stored post-boot once the lookup succeeds.

### Repo conventions
- `.agent.md` defines a custom agent for firmware work on this hardware.
- `.gitignore` excludes build output, IDE files, and generated firmware artifacts.
- `.gitattributes` keeps line endings consistent while treating binary assets as binary.
