# esp-ePaper-bin-tracker

Battery-powered firmware for the Waveshare ESP32-C6 1.54" e-Paper development board. It shows the next Auckland Council rubbish, food scraps and recycling collection days.

## Hardware
[Waveshare ESP32-C6 1.54" e-Paper AIoT Development Board](https://www.waveshare.com/esp32-c6-epaper-1.54.htm) ([docs](https://docs.waveshare.com/ESP32-C6-ePaper-1.54), [examples and schematic](https://github.com/waveshareteam/ESP32-C6-ePaper-1.54))

| Feature | Spec |
| --- | --- |
| MCU | ESP32-C6, 32-bit RISC-V up to 160MHz, 512KB HP SRAM + 16KB LP SRAM |
| Flash | 16MB external NOR flash |
| Display | 1.54" e-Paper V2, 200 × 200, black and white, SPI. Full refresh about 2s, partial refresh about 0.3s. SSD1681-style command set ([datasheet](https://files.waveshare.com/wiki/common/1.54inch_e-paper_V2_Datasheet.pdf)) |
| Wireless | Wi-Fi 6, Bluetooth 5, IEEE 802.15.4 (Zigbee/Thread) |
| USB | USB-C to the ESP32-C6's native USB-Serial-JTAG, used for flashing and logs |
| Power | MX1.25 3.7V lithium battery header with onboard charging |
| Buttons | BOOT (GPIO9) and PWR (GPIO2). There is no reset button. |
| Also on board | PCF85063 RTC, SHTC3 temperature and humidity sensor, ES8311 audio codec with speaker and mic, TF card slot, TCA9554 I/O expander |

### Pin map
Taken from Waveshare's ESP-IDF examples (`epaper_config.h`):

| Signal | Pin |
| --- | --- |
| e-Paper SPI (SPI2_HOST) | SCK GPIO6, MOSI GPIO5, CS GPIO7, DC GPIO15, RST GPIO11, BUSY GPIO10 |
| I2C bus | SDA GPIO18, SCL GPIO8 |
| TCA9554 expander (I2C 0x20) | EXIO0 e-Paper power, EXIO1 audio power, EXIO4 LED, EXIO5 battery power hold |
| Battery voltage | GPIO0 (ADC1 channel 0), through a 1:2 divider |
| TF card | shares SCK/MOSI with the display, MISO GPIO4, CS GPIO3 |
| I2C devices | PCF85063 RTC at 0x51, SHTC3 at 0x70 |

Two things about power control matter for this project:
- The e-Paper panel is powered through **TCA9554 EXIO0**. The display driver has to bring up I2C and switch that pin on before the panel responds.
- On battery, **EXIO5** holds the board's power on. Waveshare's examples drive it high at boot and drive it low to power the board off.

### Build environment check
| Item | Board | This project |
| --- | --- | --- |
| Target | ESP32-C6 | `board = esp32-c6-devkitc-1`, a generic ESP32-C6 definition |
| Flash | 16MB | `board_upload.flash_size = 16MB` and `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` in `sdkconfig.defaults` |
| ESP-IDF | Waveshare requires v5.5.0 or later | PlatformIO `espressif32` 7.0.0 ships ESP-IDF 6.0.0 |
| Upload and monitor | Native USB-Serial-JTAG | `pio run -t upload` and `pio device monitor` over USB-C |

PlatformIO reports RAM as 320KB. That is the usable figure in the generic board definition, not the chip's 512KB total, and it only affects the size report.

## Development environment
The project uses PlatformIO with the Espressif32 platform and the **ESP-IDF** framework. It does not use Arduino, so Arduino libraries such as GxEPD2 or Adafruit GFX will not compile here. The display driver has to be written against ESP-IDF APIs.

### Prerequisites
- Python 3.10+
- PlatformIO Core (`pip install platformio`), installed in the project's `.venv` or globally
- A USB-C data cable. The board uses the ESP32-C6's native USB, so no extra serial driver is needed.

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
3. Redraw the e-Paper display
4. Put the display into its own sleep mode, then put the ESP32-C6 into deep sleep for `REFRESH_INTERVAL_HOURS` (24 by default)

The e-Paper panel keeps its image with no power, so the schedule stays visible while the device sleeps. Each wake starts from `app_main` like a fresh boot. Anything that must survive a sleep, such as the resolved address ID, has to be stored in NVS or RTC memory.

### Flashing a sleeping device
The native USB connection drops while the chip is in deep sleep, so `pio run -t upload` and `pio device monitor` cannot reach it. The board has no reset button. To enter download mode, hold **BOOT** while powering the board on (unplug and replug USB-C, with the battery disconnected), then upload again. For development, set `REFRESH_INTERVAL_HOURS` low or temporarily skip `enter_deep_sleep()` in `src/main.cpp`.

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
| `src/display_manager.*` | e-Paper rendering and display sleep |
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
- The e-Paper driver, including switching on TCA9554 EXIO0 (the display currently logs to serial)
- Battery power hold (EXIO5) and battery voltage reading
