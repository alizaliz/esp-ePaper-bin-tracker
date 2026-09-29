# esp-ePaper-bin-tracker

Battery-powered firmware for an ESP32-C6 paired with a 1.54-inch Waveshare ePaper display. It shows the next Auckland Council rubbish, food scraps and recycling collection days.

## Hardware
- MCU: ESP32-C6 with 16MB flash
- Display: Waveshare 1.54-inch ePaper panel (SSD1681 controller)
- Power: battery. The device spends almost all of its time in deep sleep.

## Development environment
The project uses PlatformIO with the Espressif32 platform and the **ESP-IDF** framework. It does not use Arduino, so Arduino libraries such as GxEPD2 or Adafruit GFX will not compile here. The display driver has to be written against ESP-IDF APIs.

### Prerequisites
- Python 3.10+
- PlatformIO Core (`pip install platformio`), installed in the project's `.venv` or globally
- USB serial driver for the ESP32-C6 board

### Quick start
1. Activate the virtual environment: `source .venv/bin/activate`
2. Create your local config: `cp src/config.example.h src/config.h`
3. Edit `src/config.h` and set your Wi-Fi SSID and password, your address search query, and the refresh interval.
4. Build, flash and monitor:
   - `pio run`
   - `pio run -t upload`
   - `pio device monitor`

`src/config.h` is git-ignored so your credentials are never committed. If you add a new setting, add it to `src/config.example.h` as well.

## Power and deep sleep
The firmware does one pass each time the device wakes, then goes back to sleep:

1. Wake (cold boot or deep sleep timer)
2. Connect to Wi-Fi and fetch the collection schedule
3. Redraw the ePaper display
4. Put the display into its own sleep mode, then put the ESP32-C6 into deep sleep for `REFRESH_INTERVAL_HOURS` (24 by default)

The ePaper panel keeps its image with no power, so the schedule stays visible while the device sleeps. Each wake starts from `app_main` like a fresh boot. Anything that must survive a sleep, such as the resolved address ID, has to be stored in NVS or RTC memory.

### Flashing a sleeping device
The USB serial connection drops while the chip is in deep sleep, so `pio run -t upload` and `pio device monitor` cannot reach it. To flash it, hold **BOOT**, press and release **RESET**, then release **BOOT** to enter download mode, and upload again. For development, set `REFRESH_INTERVAL_HOURS` low or temporarily skip `enter_deep_sleep()` in `src/main.cpp`.

## Auckland Council data source
The device calls the official Auckland Council data source directly:
- https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days.html

Runtime flow:
1. Resolve the address to an ID with the address search API. This only needs to happen once; the ID should then be cached in NVS.
   - Example: `GET https://www.aucklandcouncil.govt.nz/api/address/search?query=500+Queen+Street`
2. Fetch the collection dates for that address ID.
   - Example: `GET https://www.aucklandcouncil.govt.nz/api/kerbside?address=12342478585`
3. Render the result like:
   - `Rubbish: Thursday, 24 September`
   - `Food scraps: Thursday, 24 September`
   - `Recycling: Thursday, 24 September`

## Project layout
| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Wake, refresh and deep sleep cycle |
| `src/auckland_council_client.*` | Address lookup and collection-day fetch |
| `src/display_manager.*` | ePaper rendering and display sleep |
| `src/collection_types.h` | Shared data types |
| `src/config.example.h` | Template for the git-ignored `src/config.h` |
| `sdkconfig.defaults` | ESP-IDF settings, such as the 16MB flash size. PlatformIO generates `sdkconfig.<env>` from this file. |

To change an ESP-IDF setting permanently, add it to `sdkconfig.defaults`. The generated `sdkconfig.*` files are git-ignored.

## Status
Working: the build, the 16MB flash configuration, and the deep sleep cycle.

Still placeholders:
- Wi-Fi connection
- HTTPS calls to the Council APIs, and JSON parsing
- Caching the address ID in NVS
- The ePaper driver and its SPI pin mapping (the display currently logs to serial)
