# esp-ePaper-bin-tracker

A battery-powered e-paper display that shows your next Auckland Council rubbish, recycling and food scraps collection. It runs on the [Waveshare ESP32-C6 1.54" e-Paper board](https://www.waveshare.com/esp32-c6-epaper-1.54.htm), wakes once a day to fetch the schedule, and sleeps the rest of the time.

![Labelled diagram of the screen and the board's controls](docs/images/diagram.svg)

- Shows which bins go out on the next pickup day, and when it is
- Reminds you the evening before (`TONIGHT`) and on the day (`TODAY`)
- Today's average temperature and humidity, Wi-Fi status and battery level
- Keeps working through Wi-Fi outages, and flags the schedule if it may be out of date
- Blinks the LED when the battery needs charging

## Setup

### What you need
- The Waveshare ESP32-C6 1.54" e-Paper board, and a USB-C data cable
- Optional: a 3.7V lithium battery with an MX1.25 plug (it charges over USB-C)
- A computer with Python 3.10 or later, and Chrome, Edge or Firefox

### 1. Install the tools
```sh
python3 -m venv .venv
source .venv/bin/activate
pip install platformio
```

### 2. Build and flash
Plug the board in, then:
```sh
pio run -t upload      # build and flash
pio device monitor     # optional: watch the log (Ctrl+C to exit)
```
The first build downloads the ESP-IDF toolchain and LVGL, which takes a few minutes. The firmware doesn't need any of your details built in.

### 3. Configure it from the settings page
Settings are made in a web page that talks to the board over USB, and are saved on the board.

1. Start the page: `python3 -m http.server 8765 --bind 127.0.0.1 --directory docs/config`
2. Open **http://localhost:8765** in a recent desktop Chrome, Edge or Firefox.
3. Click **Connect** and choose the bin tracker. Close `pio device monitor` first, as only one program can use the port.
4. Fill in your **Wi-Fi network**, your **Assessment number**, and your **location** for the weather. To find the Assessment number, search for your address on the [Council's collection day page](https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days.html); it's also the number at the end of the results page's address.
5. Click **Save and restart**.

The page also sets the reminder time, the low battery level, the screen orientation and more. Come back to it any time to change them. The Wi-Fi password is never shown, and stays as it is unless you type a new one.

<details>
<summary>All settings</summary>

| Setting | Default | What it does |
| --- | --- | --- |
| Wi-Fi network and password | none | Must be a 2.4GHz network |
| Assessment number | none | Identifies your address on the Council website |
| Outdoor weather | on | Off shows the onboard sensor (indoor) instead |
| Latitude and longitude | central Auckland | Location for the weather |
| Show TONIGHT from | 18:00 | Time on the evening before a pickup when the reminder appears |
| Blink the LED at TONIGHT | on | |
| Low battery reminder | 10% | The LED double-blinks below this charge. 0 turns it off. |
| USB-C end at the top | on | Rotates the screen 180° |
| Dither greyscale images | off | Shading instead of a hard threshold |
| Indoor sensor correction | 0°C | Added to the onboard sensor's temperature |
| Fallback refresh interval | 24 hours | Only used until the clock has been set |
| Stay awake on USB | on | Keeps the board awake while a computer is connected, for flashing, logs and this page |

To build your own defaults into the firmware instead, copy `src/config.example.h` to `src/config.h` (git-ignored) and edit it. Settings saved from the page override them.

</details>

### 4. Check it's working
Within about 15 seconds of restarting, the screen redraws. The log (in `pio device monitor`, or under **Device log** on the settings page) shows something like:
```
Weather (today's mean): 13.6 C, 73.0 %
Next pickup 2026-10-08: rubbish 1, recycling 1, food scraps 1
Next refresh at 2026-10-07 00:05
Computer connected over USB: staying awake for flashing, logs and the config page.
```
While it's plugged into a computer, the board stays awake. Unplug it, or run it from a battery or USB charger, and it sleeps until the next refresh.

<details>
<summary>Troubleshooting</summary>

| Problem | Fix |
| --- | --- |
| Upload can't find or connect to the board | The board is probably asleep, so its USB is off. Disconnect the battery, hold **BOOT** while unplugging and replugging USB-C, then upload again. |
| Settings page can't connect | Close `pio device monitor` or anything else using the port. If the board is asleep, press **PWR** to wake it. |
| `Failed to resolve component 'lvgl__lvgl'` | A one-off PlatformIO ordering issue on the first build after a clean. Run the build again. |
| `no Wi-Fi network set` | Set up Wi-Fi on the settings page. |
| `no connection to "<SSID>"` | Check the Wi-Fi name and password, and that the network is 2.4GHz. |
| `collection dates not found` | Check the Assessment number. If it's right, the Council website layout may have changed (see Data sources under [How it works](#how-it-works)). |
| Screen shows `--` for temperature and humidity | Neither the online weather nor the onboard sensor could be read. |

</details>

## How it works

### The screen
| Upcoming pickup | Evening before | Pickup day |
| :---: | :---: | :---: |
| ![Upcoming pickup](docs/images/screen_upcoming.png) | ![TONIGHT the evening before](docs/images/screen_tonight.png) | ![TODAY on the pickup day](docs/images/screen_today.png) |
| Rubbish and food scraps go out Thursday; recycling doesn't | From 6pm the evening before | On the day |

| Out of date | Low battery | Offline |
| :---: | :---: | :---: |
| ![Schedule out of date](docs/images/screen_stale.png) | ![Low battery](docs/images/screen_low_battery.png) | ![Offline, with no readings](docs/images/screen_offline.png) |
| The schedule may be stale | The LED also blinks | No Wi-Fi and no readings |

- **Top row:** Wi-Fi status (slashed if this refresh couldn't get online), temperature, humidity and battery charge.
- **Bins:** rubbish, recycling and food scraps. A bin with a slash through it isn't collected on the next pickup day.
- **Bottom:** the next pickup day, e.g. `THU 8`. It becomes `TONIGHT` from 6pm the evening before, then `TODAY` on the day.
- **Out of date:** a history icon next to the date means the schedule may be stale, so `TODAY` and `TONIGHT` are hidden. This happens when the pickup day has passed, the last successful fetch was over 48 hours ago, or the board lost power and couldn't get online.

Temperature and humidity are today's outdoor average from Open-Meteo. If that can't be fetched, the onboard sensor's indoor reading is used instead.

### Daily routine
The board spends almost all its time in deep sleep. The e-paper screen keeps its image with no power, so the schedule stays visible.

| When | What happens |
| --- | --- |
| **00:05** every night | Full refresh: connect to Wi-Fi, set the clock, fetch the schedule and weather, redraw the screen, then sleep. Takes about 10 seconds. |
| **18:00** the evening before a pickup | Full refresh showing `TONIGHT`, and a slow blink of the LED for 10 seconds |
| After a failed refresh | Retry in an hour, up to three times, then return to the normal schedule |
| Every 10 minutes, while the battery is low | Double-blink the LED for 10 seconds, then sleep again. No Wi-Fi or redraw. |
| PWR press, power on or reset | Full refresh straight away |

Any wake that runs longer than 90 seconds, for example because of a hung network request, is cut short with a retry due in an hour.

### LED and buttons
| | |
| --- | --- |
| **Green LED, slow blink** | Bin night: put the bins out |
| **Green LED, double-blink** | Battery below 10%: charge it. Stops once it's back above 15%. |
| **Red LED** | Charging. This is controlled by the board's charger, not the firmware. |
| **PWR button** | Press to restart and refresh now. On battery, it also turns the board on. |
| **BOOT button** | Hold while powering on to enter download mode for flashing |

### When things go wrong
| Situation | What you see |
| --- | --- |
| Wi-Fi down | The last schedule, a slashed Wi-Fi icon and indoor readings. Retries hourly. |
| Offline for days, or the pickup day has passed | The history icon next to the date |
| Power loss | The last schedule is saved in flash, so it's redrawn as soon as power returns |
| Council website changed | The fetch fails and the last schedule stays on screen. The parser may need updating. |

<details>
<summary>Data sources</summary>

- **Collection days:** the Council's collection day page for your address, `.../rubbish-recycling-collection-days/<assessment number>.html`. The page is about 2.7MB, but the household collection dates are in the first ~16KB, so the board stops reading once it has them. Dates come without a year, such as `Thursday, 8 October`, so the year is taken as the one in which that date falls on that weekday. This relies on the page's HTML rather than a published API, so a site redesign can break it.
- **Weather:** [Open-Meteo](https://open-meteo.com) daily mean temperature and humidity for your location, in Auckland time. It's free and needs no API key.
- **Time:** NTP from `nz.pool.ntp.org`, on every wake that gets online. The clock keeps running through deep sleep.

</details>

<details>
<summary>Battery and power</summary>

- The battery voltage is read through the board's divider on GPIO0 and converted to a percentage with a lithium polymer discharge curve. With no battery connected, the charger's output reads as full.
- Every wake keeps the board's battery power hold switched on. The panel is powered only while it's being redrawn.
- The low battery LED uses light sleep between blinks, so the 10-minute reminder wakes cost little.
- Deep sleep turns off USB, so a sleeping board can't be flashed or configured. It stays awake while a computer is connected; turn off **Stay awake on USB** to test real sleep while plugged in. Chargers and power banks don't count as a computer.

</details>

## Development

<details>
<summary>Project layout</summary>

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Wake, refresh, scheduling and deep sleep |
| `src/auckland_council_client.*`, `src/council_parser.*` | Fetch and parse the collection day page |
| `src/weather_client.*` | Daily average weather from Open-Meteo |
| `src/network.*`, `src/https_client.*` | Wi-Fi, NTP and streaming HTTPS |
| `src/storage.*` | Values saved in flash (NVS): the last schedule and the settings |
| `src/settings.*` | Settings: `config.h` defaults overridden by values saved from the page |
| `src/config_service.*` | Answers the settings page over USB serial |
| `docs/config/index.html` | The settings page (Web Serial) |
| `src/display_manager.*`, `src/epd_ssd1681.*` | LVGL setup and the e-paper panel driver |
| `src/ui/bin_screen.*` | Screen layout: plain LVGL, previewable on a computer |
| `src/ui/mono_convert.*` | Greyscale to black and white, and the 180° flip |
| `src/shtc3.*`, `src/battery.*` | Onboard sensor and battery reading |
| `src/board_power.*`, `src/board_pins.h` | I2C, the I/O expander (panel power, battery hold, LED) and pins |
| `components/fonts/` | Generated LVGL fonts: Montserrat Bold and Font Awesome icons |
| `tools/preview/` | Renders the screen to PNGs on a computer |
| `tools/fonts/generate.sh` | Regenerates the fonts |
| `tools/docs/make_images.py` | Regenerates the README images in `docs/images/` |
| `sdkconfig.defaults`, `partitions.csv` | ESP-IDF settings and the flash layout (4MB app partition) |

</details>

<details>
<summary>Previewing the screen and updating images</summary>

```sh
pio run                              # once, so LVGL is downloaded
./tools/preview/run.sh               # writes PNGs to tools/preview/out/
python3 tools/docs/make_images.py    # refreshes docs/images/ and the diagram
```
The preview uses the same layout code, fonts and black and white conversion as the firmware. It warns if anything is drawn off screen. It needs a C/C++ compiler and zlib, both included with macOS.

</details>

<details>
<summary>Fonts, icons and images</summary>

- Fonts are compiled into the firmware as bitmaps and contain only the characters the screen uses. To add characters, icons or sizes, edit `tools/fonts/generate.sh` and run it. It needs Node.js. Then add any new font to `components/fonts/CMakeLists.txt` and `fonts.h`.
- PNG images can be embedded from `src/assets/`. Add the file to `board_build.embed_files` in `platformio.ini` and declare it in `src/assets.h`. Keep them icon-sized: they're decoded to full colour in RAM.
- `DISPLAY_DITHERING` chooses between a crisp threshold (best for text and icons) and dithered shading (best for photos).

</details>

<details>
<summary>Hardware notes</summary>

Board specs, pinout and schematic: [Waveshare docs](https://docs.waveshare.com/ESP32-C6-ePaper-1.54) and [examples](https://github.com/waveshareteam/ESP32-C6-ePaper-1.54). Details that matter to this firmware:

- The e-paper panel is powered through I/O expander pin EXIO0, and the battery power hold is on EXIO5. The expander's registers are written directly and never reset, because a reset would briefly release the power hold.
- The green LED is on EXIO4 and is active low. The red LED belongs to the charger.
- The PWR button (GPIO2) can wake the chip from deep sleep. BOOT (GPIO9) can't, so the firmware doesn't use it.
- The project uses ESP-IDF, not Arduino, through PlatformIO. LVGL and cJSON are ESP-IDF components; `dependencies.lock` pins their versions.

</details>

## Licences
- Project code: MIT (see `LICENSE`)
- Montserrat font: SIL Open Font License 1.1
- Font Awesome Free icons: CC BY 4.0; font files SIL Open Font License 1.1 ([fontawesome.com/license/free](https://fontawesome.com/license/free))
- Weather data: [Open-Meteo](https://open-meteo.com), CC BY 4.0, free for non-commercial use
