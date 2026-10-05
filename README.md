# esp-ePaper-bin-tracker

Battery-powered firmware for the Waveshare ESP32-C6 1.54" e-Paper development board. It shows the next Auckland Council rubbish, food scraps and recycling collection days.

![Labelled diagram of the screen and the board's controls](docs/images/diagram.svg)

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
The project uses PlatformIO with the Espressif32 platform and the **ESP-IDF** framework. It does not use Arduino, so Arduino libraries such as GxEPD2 or Adafruit GFX will not compile here.

The one external dependency, [LVGL](https://lvgl.io) 9 (graphics), is an ESP-IDF component declared in `src/idf_component.yml`. The build downloads it into `managed_components/` (git-ignored), and `dependencies.lock` pins the exact version, so commit that file.

### Prerequisites
- Python 3.10+
- PlatformIO Core (`pip install platformio`), installed in the project's `.venv` or globally
- A USB-C data cable. The board uses the ESP32-C6's native USB, so no extra serial driver is needed.

### Quick start
1. Activate the virtual environment: `source .venv/bin/activate`
2. Create your local config: `cp src/config.example.h src/config.h`
3. Edit `src/config.h`: set your Wi-Fi SSID and password, your Council address ID (see [Auckland Council data source](#auckland-council-data-source)), and your latitude and longitude for the weather.
4. Build, flash and monitor:
   - `pio run`
   - `pio run -t upload`
   - `pio device monitor`

`src/config.h` is git-ignored so your credentials are never committed. If you add a new setting, add it to `src/config.example.h` as well.

If the first build after a clean, or after deleting `sdkconfig.esp32-c6-devkitc-1`, fails with `Failed to resolve component 'lvgl__lvgl'`, run `pio run` again. It is a one-off ordering issue while PlatformIO sets up the component manager.

## Display and graphics

### Screen layout
| Upcoming pickup | Bin night | Pickup day |
| :---: | :---: | :---: |
| ![Upcoming pickup](docs/images/screen_upcoming.png) | ![TONIGHT the evening before](docs/images/screen_tonight.png) | ![TODAY on the pickup day](docs/images/screen_today.png) |
| Rubbish and food scraps go out on Thursday; recycling doesn't | Put all three bins out tonight | All three bins go out today |

| Low battery | Offline |
| :---: | :---: |
| ![Low battery](docs/images/screen_low_battery.png) | ![Offline, with no readings](docs/images/screen_offline.png) |
| Battery almost empty; the green LED also blinks | No Wi-Fi and no sensor reading |

These are host previews, rendered by the same layout code and fonts as the firmware. On the device, the image is rotated 180° (`DISPLAY_FLIP`) so the USB-C and LED end of the board is at the top.

- **Top row, centred:** Wi-Fi status, temperature (°C), relative humidity (%) and battery charge.
  - The Wi-Fi icon shows whether this refresh got online. A slash through it means the connection failed, so the schedule shown is the last one fetched.
  - Temperature and humidity are the current outdoor weather from [Open-Meteo](https://open-meteo.com) for `WEATHER_LATITUDE`/`WEATHER_LONGITUDE`. If Wi-Fi or the request fails, the onboard SHTC3 sensor (indoor) is used instead, and if that fails too it shows `--`. Set `USE_ONLINE_WEATHER` to `0` to always use the sensor.
  - The sensor is read first thing on wake, before Wi-Fi and the display warm the board. If its readings run warm, set `TEMPERATURE_OFFSET_C`.
  - The battery glyph's fill is proportional to the charge. It's updated with each redraw, so once a day.
- **Middle:** the next pickup day in bold, e.g. `THU 8`. From `BIN_NIGHT_HOUR` (18:00) the evening before, it shows `TONIGHT` as a reminder to put the bins out, and the green LED blinks for 10 seconds (`BIN_NIGHT_LED`). On the pickup day itself it shows `TODAY`. Both are in the same bold white text on a black strip.
- **Bottom:** three bin icons (rubbish, recycling, food scraps), each with a tick if it goes out on that pickup day or a cross if it doesn't.

`TODAY` needs the current date. The clock is set from NTP (`nz.pool.ntp.org`) on every wake that gets online, and keeps running through deep sleep in between.

The layout is in `src/ui/bin_screen.cpp`. It is plain LVGL code, so it can be previewed on a computer (see below).

### Rendering
Screens are laid out with LVGL. `DisplayManager` renders the whole 200 × 200 screen once in 8-bit greyscale, converts it to the panel's 1-bit format, and sends it to the panel in a single full refresh. The panel driver (`src/epd_ssd1681.*`) is a small ESP-IDF driver based on Waveshare's examples. It uses the panel's built-in full-refresh waveform, since the display only updates once per wake.

**Text and icons.** LVGL fonts are compiled into the firmware as bitmap arrays. The screen uses fonts generated by `tools/fonts/generate.sh` into `components/fonts/`:
- Montserrat Bold for the pickup day and the readings. LVGL's built-in Montserrat has no bold weight and no `°` sign.
- [Font Awesome Free](https://fontawesome.com) solid icons: bin, recycle, apple, thermometer, droplet, tick and cross. Icons drawn as font glyphs stay crisp in black and white at any size.

Each font only contains the characters the screen uses. To add characters, icons or sizes, edit `tools/fonts/generate.sh` and run it from the repo root; it needs Node.js. Add any new font to `components/fonts/CMakeLists.txt` and `fonts.h`. The fonts live in their own component because PlatformIO compiles C files in `src/` with C++-only flags, which fails.

LVGL's regular Montserrat sizes 14, 16 and 20 are also enabled in `sdkconfig.defaults` (`CONFIG_LV_FONT_MONTSERRAT_<size>=y`).

**Images.** PNG files are embedded directly, with no conversion step:
1. Put the PNG in `src/assets/`.
2. Add its path to `board_build.embed_files` in `platformio.ini`.
3. Declare it in `src/assets.h` with `ASSET_PNG(my_icon_png)` (the file name with `.` replaced by `_`).
4. Draw it: `static lv_image_dsc_t icon = assetImage(my_icon_png_start, my_icon_png_end);` then `lv_image_set_src(img, &icon);`

LVGL's PNG decoder unpacks the image to 32-bit colour in RAM while drawing, so keep embedded PNGs icon-sized. A full-screen 200 × 200 image needs about 160KB. For large images, convert them to a C array in LVGL's `L8` or `I1` format with the [LVGL image converter](https://lvgl.io/tools/imageconverter) instead.

**Black and white conversion.** The panel shows only black and white. `DISPLAY_DITHERING` in `src/config.h` controls how greyscale becomes 1-bit:
- `0` (default): a plain threshold. It keeps text and line icons crisp; greys become black or white.
- `1`: Floyd–Steinberg dithering. Photos and gradients show as shading, but text edges get slightly speckled.

For the sharpest results with either setting, design icons in pure black and white.

### Previewing the screen on a computer
`tools/preview/run.sh` renders the screen layout to PNGs in `tools/preview/out/`, using the same LVGL code, fonts and black and white conversion as the firmware. It covers an upcoming pickup, `TODAY`, the widest possible date and readings, a low battery, and offline with no readings. It also prints a warning if anything is drawn off screen. Use it to check a layout change before flashing. Afterwards, run `python3 tools/docs/make_images.py` to refresh the README images in `docs/images/`, including the labelled diagram. It needs a C/C++ compiler and zlib, which macOS includes, and LVGL in `managed_components/`, so run `pio run` once first. The first run compiles LVGL and takes about a minute.

## Buttons
- **PWR (GPIO2):** a short press restarts the device and runs a full refresh (Wi-Fi, fetch, redraw). It works at any time, including from deep sleep: GPIO2 is one of the ESP32-C6's low-power GPIOs, so it can wake the chip. On battery, PWR also turns the board on, which is handled in hardware. Waking from deep sleep restarts the firmware from `app_main`; while the board is awake on USB, a press calls `esp_restart()`.
- **BOOT (GPIO9):** hold it while powering on to enter download mode. The firmware doesn't use it: GPIO9 can't wake the chip from deep sleep, so it would only work in the few seconds a day the board is awake.

A restart isn't a power cycle. On USB power there's no way for the firmware to cut power. On battery, releasing the power hold (EXIO5) would switch the board off completely, but PWR would then be needed to turn it back on.

## Battery
The battery voltage is read on GPIO0 through the board's 200k/200k divider, using ESP-IDF's calibrated ADC, averaged over 16 samples. It's converted to a charge percentage with a typical lithium polymer discharge curve (4.20V = 100%, 3.70V ≈ 31%, 3.30V = 0%). Readings while charging run high, and with no battery connected the charger's output on the battery pin reads as about 4.2V, so the glyph shows full while USB is plugged in. The board's red LED is driven by the charger chip and shows charging. It isn't connected to the ESP32-C6, so the firmware can't tell when the battery is charging.

**Low battery reminder:** below `LOW_BATTERY_PERCENT` (10% by default), the board wakes every 10 minutes and blinks the green LED (TCA9554 EXIO4, active low) once a second for 10 seconds. These wakes don't use Wi-Fi or redraw the screen; the daily refresh still happens on schedule. Light sleep between blinks keeps their cost down. Blinking stops once the charge is back above `LOW_BATTERY_PERCENT` + 5%, so it doesn't flicker on and off around the threshold.

## Power and deep sleep
The firmware does one pass each time the device wakes, then goes back to sleep:

1. Wake (cold boot or deep sleep timer)
2. Read the onboard temperature and humidity sensor
3. Connect to Wi-Fi (15 second timeout), set the clock from NTP, fetch the weather and the collection schedule, then switch Wi-Fi off
4. Power the panel on (TCA9554 EXIO0), redraw it, put it into its own deep sleep, and power it off again. If the fetch failed, the last fetched schedule (kept in RTC memory) is redrawn with fresh readings. If there has never been a successful fetch, the panel is left alone.
5. If the battery is low, or it's bin night, blink the LED for 10 seconds.
6. Arm the PWR button as a wake source, waiting for it to be released first.
7. Put the ESP32-C6 into deep sleep until the next refresh: 00:05 Auckland time, so `TODAY` is shown for the whole pickup day, or `BIN_NIGHT_HOUR` the evening before a pickup if that comes first. If the clock hasn't been set, the next refresh is `REFRESH_INTERVAL_HOURS` away instead. While the battery is low it wakes every 10 minutes to blink, then sleeps again until the refresh is due.

**Failed refreshes:** if Wi-Fi or the schedule fetch fails, the next refresh is an hour later instead, for up to three retries in a row, before falling back to the normal schedule.

**Time limit:** any wake that runs longer than 90 seconds, for example because of a hung network request, is cut short and the board goes into deep sleep with a retry due in an hour. The limit is lifted while a computer is connected over USB. If the limit is hit during a screen refresh, the panel's power may stay on until the next wake.

Power on, reset, flashing and a PWR press always run a full refresh.

The battery power hold (TCA9554 EXIO5) is driven high on every wake. The expander's registers are written directly and never reset, because the expander stays powered through deep sleep. A reset would briefly release the hold pin and could cut power on battery.

The e-Paper panel keeps its image with no power, so the schedule stays visible while the device sleeps. Each wake starts from `app_main` like a fresh boot. Anything that must survive a sleep has to be kept in RTC memory or NVS. The last fetched schedule is kept in RTC memory, so it survives deep sleep but not a power loss.

### Flashing and serial logs
Deep sleep turns off the native USB port. To keep development simple, the firmware stays awake after refreshing for as long as a computer is connected over USB, then goes to sleep when it's unplugged. While plugged in, `pio run -t upload` and `pio device monitor` work normally. A USB charger or power bank doesn't count as a connected computer, so battery behaviour is unchanged. Set `STAY_AWAKE_ON_USB` to `0` in `src/config.h` to test real deep sleep while plugged in.

If the board is already asleep, for example after running on battery, or is running firmware without this feature, use download mode. The board has no reset button: disconnect the battery, hold **BOOT** while unplugging and replugging USB-C, then upload.

## Auckland Council data source
The schedule comes from the Council's collection day page for your address:

`https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days/<address ID>.html`

**Finding your address ID:** search for your address on the [collection day page](https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days.html). The results page URL ends in `/<address ID>.html`. Put that number in `COUNCIL_ADDRESS_ID`. The Council's address search API needs a login token, so the device can't look the ID up itself.

**How it's read:** the page is about 2.7MB, but the "Household collection" block with the next dates for rubbish, food scraps and recycling is in the first ~16KB. The device streams the page, stops once that block has arrived (capped at 96KB), and closes the connection. Dates look like `Thursday, 8 October` with no year, so the year is chosen as the one, closest to the current year, on which that date falls on that weekday. This handles December to January.

The next pickup is the earliest of the three dates, and each bin collected on that date gets a tick. `src/council_parser.cpp` does the parsing and has no ESP-IDF dependencies.

This relies on the page's HTML, not a published API, so a Council website redesign can break it. If that happens, the log shows `collection dates not found` and the screen keeps the last fetched schedule.

## Project layout
| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Wake, refresh and deep sleep cycle |
| `src/auckland_council_client.*` | Fetches the collection day page |
| `src/council_parser.*` | Extracts the next collection dates from the page |
| `src/weather_client.*` | Current weather from Open-Meteo |
| `src/network.*` | Wi-Fi connection and NTP clock sync |
| `src/https_client.*` | Streaming HTTPS GET with certificate checking |
| `src/display_manager.*` | LVGL setup and pushing rendered screens to the panel |
| `src/ui/bin_screen.*` | Screen layout (plain LVGL, previewable on a computer) |
| `src/ui/mono_convert.*` | Greyscale to 1-bit conversion (threshold or dithering) |
| `src/shtc3.*` | SHTC3 temperature and humidity sensor driver |
| `src/battery.*` | Battery voltage and charge percentage |
| `components/fonts/` | Generated LVGL fonts (Montserrat Bold, Font Awesome icons) |
| `tools/fonts/generate.sh` | Regenerates `components/fonts/` |
| `tools/preview/` | Host preview of the screen layout |
| `tools/docs/make_images.py`, `docs/images/` | README images: screen previews and the labelled diagram |
| `src/epd_ssd1681.*` | e-Paper panel driver (SPI) |
| `src/board_power.*` | Shared I2C bus and TCA9554 outputs (panel power, battery hold, LED) |
| `src/board_pins.h` | Board pin map |
| `src/assets.h`, `src/assets/` | Embedded PNG images |
| `src/idf_component.yml` | ESP-IDF component dependencies (LVGL, cJSON) |
| `src/collection_types.h` | Shared data types |
| `src/config.example.h` | Template for the git-ignored `src/config.h` |
| `sdkconfig.defaults` | ESP-IDF and LVGL settings, such as the 16MB flash size and enabled fonts. PlatformIO generates `sdkconfig.<env>` from this file. |
| `partitions.csv` | Flash layout: a 4MB app partition, leaving the rest of the 16MB free for OTA or storage later |

To change an ESP-IDF setting permanently, add it to `sdkconfig.defaults`. The generated `sdkconfig.*` files are git-ignored.

## Status
Working and tested on hardware: Wi-Fi, NTP clock sync, the Council schedule fetch, Open-Meteo weather with the sensor fallback, the bin screen layout, deep sleep, panel power and the battery hold, and staying awake on USB.

Also tested on hardware: retrying after a failed fetch (with an invalid address ID) and the 90 second time limit (temporarily cut to 5 seconds). Not yet tested on hardware: the 00:05 and bin night wakes and the `TONIGHT` and `TODAY` screens, which depend on the date. The bin night date maths was checked on a computer across month, year and daylight saving boundaries.

Also tested on hardware: the battery reading, the low battery LED blink (by temporarily raising the threshold), and the PWR button restart, both while awake and from deep sleep. Not yet tested: a real low battery over several 10-minute wakes.

## Licences
- Project code: MIT (see `LICENSE`)
- Montserrat font: SIL Open Font License 1.1
- Font Awesome Free icons: CC BY 4.0, font files SIL Open Font License 1.1 ([fontawesome.com/license/free](https://fontawesome.com/license/free))
- Weather data: [Open-Meteo](https://open-meteo.com), CC BY 4.0. Free for non-commercial use.
