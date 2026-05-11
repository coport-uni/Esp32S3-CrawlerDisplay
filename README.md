# ESP32-S3-BOX-3 Sensor Self-Test Firmware

A diagnostic firmware for the [ESP32-S3-BOX-3](https://github.com/espressif/esp-box) development kit and its [BOX-3-SENSOR](https://www.espressif.com/en/products/devkits) extension board. Boots into a six-tab LVGL dashboard on the on-board 320×240 LCD and exercises every peripheral so you can confirm the board is healthy before building on top of it.

| Tab | What it shows |
|-----|---------------|
| **IMU** | Live accelerometer (g), gyroscope (dps), and tilt angle from the on-board ICM42670 |
| **Env** | Temperature / humidity from the AHT30 on the SENSOR extension |
| **Radar** | AT581X presence detection events + count |
| **Audio** | ES7210 mic RMS bar + "Beep" button that plays a 1 kHz tone through the ES8311 speaker |
| **IR** | RX pulse count + "Send Test" NEC frame over the on-board IR diodes |
| **Btn** | Short / long press counters for CONFIG, MUTE, MAIN buttons |

This README walks an absolute beginner from a fresh Windows PC to flashing the firmware. If you have already used ESP-IDF, jump straight to [Build, Flash, Monitor](#5-build-flash-monitor).

---

## Hardware required

- **ESP32-S3-BOX-3** main board (rev with ICM42670 IMU; revs ship with either GT911 or TT21100 touch controller — both supported by the BSP)
- **ESP32-S3-BOX-3-SENSOR** extension (provides AHT30, AT581X radar, IR LEDs; plugs into the PMOD1 connector). Without it the Env / Radar / IR tabs stay idle — the IMU / Audio / Btn tabs still work.
- A **USB-C cable that carries data** (a charge-only cable is the #1 cause of "device not detected")

The BOX-3 main board has **no I²C pull-ups on the PMOD1 dock bus** — they live on the SENSOR extension. Make sure the extension is seated firmly before debugging Env / Radar issues.

---

## 1. Install VSCode

Download from <https://code.visualstudio.com/> and install with defaults. No special options needed.

## 2. Install the official ESP-IDF VSCode extension

In VSCode, open the Extensions side panel (`Ctrl+Shift+X`) and search for **"ESP-IDF"** — the publisher must be **Espressif Systems**. Install it.

The extension is what bridges VSCode and the ESP-IDF toolchain; it owns the build/flash/monitor commands and the Kconfig editor.

## 3. Configure ESP-IDF (one time, ~10–20 minutes)

Open the Command Palette (`Ctrl+Shift+P`) → **`ESP-IDF: Configure ESP-IDF Extension`** → choose **Express** install with:

- ESP-IDF version: **v6.0.1** (or any v5.3+)
- ESP-IDF Tools directory: `C:\Espressif\tools` (default)
- Python environment: let the extension create one

Wait for the green "Configuration complete" message. After this you never have to source `export.ps1` manually — the extension handles it.

## 4. Get the source and Windows USB drivers

```powershell
git clone https://github.com/coport-uni/Esp32S3-CrawlerDisplay.git
cd Esp32S3-CrawlerDisplay
code .
```

When VSCode prompts to trust the workspace, accept. The extension will detect the project type automatically.

### Windows USB driver setup (Zadig, one time per host)

The ESP32-S3 USB-C port exposes a **composite device** with two interfaces — one CDC (virtual COM for `monitor`), one JTAG (for `flash` / debugger). Windows does not always bind the correct driver to each.

1. Plug in the BOX-3 via USB-C.
2. Download [Zadig](https://zadig.akeo.ie/) and run it.
3. `Options` → **List All Devices**.
4. For the **CDC** interface, keep / install **USB Serial (CDC)**. A `COMxx` should appear in Device Manager.
5. For the **JTAG** interface, install **WinUSB v6.x**. Do **not** replace the CDC half with WinUSB.
6. Confirm in Device Manager: one entry under *Ports (COM & LPT)*, one entry under *Universal Serial Bus devices* with `WinUSB`.

Symptoms of getting Zadig wrong:
- No COM port appears → CDC interface mis-driven.
- `idf.py flash` complains about libusb / cannot open device → JTAG interface mis-driven.

## 5. Build, Flash, Monitor

The extension binds every common action to a chord shortcut starting with `Ctrl+E`. Use these inside any source file of this project:

| Shortcut | What it does |
|----------|--------------|
| `Ctrl+E T` | Set target → choose **esp32s3** (only required once) |
| `Ctrl+E P` | Select serial port → pick the COM number that appeared after Zadig |
| `Ctrl+E B` | Build only |
| `Ctrl+E F` | Flash only |
| `Ctrl+E M` | Open serial monitor (exit with `Ctrl+]`) |
| `Ctrl+E D` | **Build + Flash + Monitor in one shot** — the everyday command |
| `Ctrl+E G` | Open the graphical menuconfig (Kconfig) |
| `Ctrl+Shift+P` → `ESP-IDF: …` | Anything else (full clean, reconfigure, etc.) |

First-time sequence: `Ctrl+E T` → `Ctrl+E P` → `Ctrl+E D`. After that, only `Ctrl+E D` for every iteration.

Expected first-boot output (truncated):
```
I (xxx) main: BOX-3 + SENSOR self-test starting
I (xxx) ili9341: LCD panel create success
I (xxx) ESP-BOX-3: Setting LCD backlight: 100%
I (xxx) ui: tabview UI created
I (xxx) SENSOR_HUB: Sensor created, ... sensor_hub_aht30
I (xxx) ES7210: Enable ES7210_INPUT_MIC1 / MIC2
I (xxx) ES7210: Unmuted
I (xxx) main: init complete
```
The LCD should now show six tabs across the top.

---

## Project layout

```
Espress_dev/
├── main/
│   ├── main.c            # app_main: init order (I2C → display → sensors → tasks)
│   ├── ui.c, ui.h        # LVGL tabview + per-tab widgets, thread-safe update helpers
│   ├── sensors.c, .h     # IMU polling task, AHT30 sensor_hub wiring, AT581X radar ISR
│   ├── audio_check.c, .h # ES7210 mic RMS task, ES8311 beep task
│   ├── ir_check.c, .h    # RMT-based IR RX/TX
│   ├── buttons_check.c   # Physical buttons via espressif/button component
│   ├── CMakeLists.txt    # Component sources + esp-box-3 BSP dependency
│   └── idf_component.yml # Managed components (BSP, ICM42670)
├── sdkconfig.defaults    # Hardware-specific Kconfig (16 MB flash, octal PSRAM, LVGL float, …)
├── managed_components/   # Auto-pulled libraries — DO NOT EDIT BY HAND
├── CLAUDE.md             # Coding rules + initialization order documentation
├── LearnedPatterns.md    # Bugs we hit and how we found them (read this when stuck)
├── ToDo.md               # Project history (append-only)
└── README.md             # This file
```

`app_main` initialization order is non-negotiable (see [CLAUDE.md](CLAUDE.md) "Initialization order"): I²C → display → backlight → UI under display lock → sensors → audio → tasks. Touching this order breaks LVGL or panics during boot.

---

## Common pitfalls

These cost us real time and are documented in detail with file/line references in [LearnedPatterns.md](LearnedPatterns.md):

- **Accelerometer / gyro display shows just `f`** — LVGL's builtin `sprintf` strips `%f` unless `CONFIG_LV_USE_FLOAT=y`. Already set in `sdkconfig.defaults` (and `sdkconfig`).
- **AHT30 Temp / Hum stays at `---`** — the BSP routes humiture through `i2c_dock_handle` (GPIO 40 / 41), which is the PMOD1 connector. Without the SENSOR extension plugged in there is no sensor to talk to.
- **AHT30 reachable but callback never fires** — `iot_sensor_handler_register_with_type` binds to the macro event base, while the polling task posts to a per-instance dynamic base. Use `iot_sensor_handler_register(handle, cb, NULL)` instead. ([sensors.c](main/sensors.c) follows this rule already.)
- **Mic RMS bar stuck at 0** — `esp_codec_dev_read` / `_write` return `ESP_CODEC_DEV_OK` (= 0) on success, **not** the byte count. Compare against `ESP_CODEC_DEV_OK`, not `> 0`.
- **`idf.py flash` cannot find the chip** — usually the USB-C cable is power-only, or Zadig drivers are swapped.
- **The `f` literal output, the missing AHT events, the stuck RMS bar all share one pattern**: the build is clean and logs look healthy, but a library convention silently drops the data path. When something "just doesn't update" with no error, suspect a return-value or event-base convention before suspecting hardware.

---

## Adding your own widget / sensor

1. Read [CLAUDE.md](CLAUDE.md) §2 (style) and §7 (research-before-coding).
2. Add the new sensor's component to [main/idf_component.yml](main/idf_component.yml), run `idf.py reconfigure` (or just `Ctrl+E B`).
3. Open the component's `.h` in `managed_components/<vendor>__<name>/include/` and confirm the real function signatures — do **not** copy from memory or from another project.
4. Create the init / polling code in a new module under `main/` (e.g. `main/mysensor.c`); follow the file pattern of [sensors.c](main/sensors.c).
5. Add a tab in [ui.c](main/ui.c) — every LVGL call from outside the LVGL task **must** be wrapped in `bsp_display_lock` / `bsp_display_unlock` (the `UI_WITH_LOCK` macro does this).
6. Append a dated section to [ToDo.md](ToDo.md), then check items off as you go.
7. When the work is done, distill any new gotcha into [LearnedPatterns.md](LearnedPatterns.md).

---

## License

See per-component licenses under `managed_components/`. Application source under `main/` is unencumbered — use as a reference for your own BOX-3 projects.
