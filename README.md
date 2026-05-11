# ESP32-S3-BOX-3 Beszel Monitor

A LVGL dashboard for the [ESP32-S3-BOX-3](https://github.com/espressif/esp-box) that polls a self-hosted [Beszel](https://beszel.dev/) (PocketBase) instance over HTTP and renders **one tab per monitored host** on the 320×240 LCD. Each tab shows the host's live CPU, memory and GPU usage as horizontal bars, plus uptime and an UP/DOWN status indicator.

This README walks an absolute beginner from a fresh Windows PC to flashing the firmware. If you have already used ESP-IDF, jump straight to [Build, Flash, Monitor](#5-build-flash-monitor).

---

## Screen layout

```
┌─────────────────────────────────────┐
│ H200Server │ 3090Server │ ...       │ ← tab bar (one per Beszel host)
├─────────────────────────────────────┤
│ ● UP                  Up 42d 5h     │
│ CPU  ██████░░░░░░░░░░░░░░  12%      │
│ MEM  ████░░░░░░░░░░░░░░░░   4%      │
│ GPU  ███░░░░░░░░░░░░░░░░░░  18%     │
│                                     │
├─────────────────────────────────────┤
│ updated 4s ago                      │ ← status footer (WiFi / auth / poll state)
└─────────────────────────────────────┘
```

- **Tab name** = Beszel host name (e.g. `H200Server`).
- **`CONFIG` / `MUTE`** buttons cycle active tab (prev / next). Touch swipe works natively.
- **Status footer**: `WiFi connecting…` / `auth failed (menuconfig)` / `updated Ns ago` / `stale Ns`.

---

## Hardware required

- **ESP32-S3-BOX-3** main board (any revision — both GT911 and TT21100 touch controllers are supported by the BSP)
- A **USB-C cable that carries data** (a charge-only cable is the #1 cause of "device not detected")
- A reachable **Beszel** server on the same WiFi network (HTTP / HTTPS both work; the project defaults to plain HTTP)

The previous board self-test firmware (which also exercised the BOX-3-SENSOR extension's IMU, AHT30, AT581X radar, IR, and audio peripherals) is preserved as a frozen reference under [`sensor_example/`](sensor_example/) — see [Prior firmware: sensor_example/](#prior-firmware-sensor_example) below.

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

## 5. Configure WiFi + Beszel credentials

The build needs your WiFi SSID/password and a Beszel user/password before anything will show on screen. Open the Kconfig editor:

```
Ctrl+E G                # or Command Palette → "ESP-IDF: SDK Configuration editor"
```

Navigate to `(Top) → Beszel monitor` and fill in:

| Option | Example value |
|---|---|
| WiFi SSID | `home-2.4g` |
| WiFi password | `…` |
| Beszel base URL | `http://10.16.21.197:8090` |
| Beszel identity | `you@example.com` |
| Beszel password | `…` |
| Poll interval (seconds) | `5` (default) |
| Max hosts cached | `16` (default) |

Save and close. Values land in the **local `sdkconfig`** file, which is git-ignored — credentials never get committed. `sdkconfig.defaults` only carries non-secret hardware defaults (PSRAM, flash size, etc.) and stays tracked.

## 6. Build, Flash, Monitor

The extension binds every common action to a chord shortcut starting with `Ctrl+E`:

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

First-time sequence: `Ctrl+E T` → `Ctrl+E P` → `Ctrl+E G` (credentials) → `Ctrl+E D`. After that, only `Ctrl+E D` for every iteration.

Expected first-boot output (truncated):
```
I (xxx) main: Beszel monitor starting
I (xxx) ESP-BOX-3: Setting LCD backlight: 100%
I (xxx) ui: ui ready
I (xxx) network: starting wifi, ssid="..."
I (xxx) network: got IP 192.168.x.x
I (xxx) beszel: auth OK (token len=NNN)
I (xxx) beszel: raw systems response (NNN bytes), first chunk follows:
I (xxx) beszel:  {"items":[ ... full JSON of every monitored host ... ]}
I (xxx) beszel: K systems parsed
```

The LCD then shows one tab per Beszel host.

---

## Project layout

```
Espress_dev/
├── main/
│   ├── main.c            # app_main: bsp → ui_create → buttons → network → beszel
│   ├── ui.c, ui.h        # dynamic per-host tabview + status footer (UI_WITH_LOCK guarded)
│   ├── network.c, .h     # non-blocking WiFi STA + auto-reconnect task
│   ├── beszel.c, .h      # PocketBase REST client + 5s poll task + host cache
│   ├── buttons_check.c, .h  # CONFIG / MUTE physical buttons → tab nav callbacks
│   ├── Kconfig.projbuild # menuconfig entries for WiFi + Beszel credentials
│   ├── CMakeLists.txt    # SRCS + REQUIRES (esp_wifi, esp_http_client, ...)
│   └── idf_component.yml # Managed components (esp-box-3 BSP, espressif/cjson)
├── sensor_example/       # frozen snapshot of the prior BOX-3 self-test (read-only)
├── sdkconfig.defaults    # hardware Kconfig (16 MB flash, octal PSRAM, LVGL float, …)
├── managed_components/   # auto-pulled libraries — DO NOT EDIT BY HAND
├── CLAUDE.md             # coding rules + initialization order documentation
├── LearnedPatterns.md    # bugs we hit and how we found them (read when stuck)
├── ToDo.md               # append-only project history
└── README.md             # this file
```

`app_main` initialization order is non-negotiable (see [CLAUDE.md](CLAUDE.md) "Initialization order"): I²C → display → backlight → UI under display lock → buttons → network → Beszel. Touching this order risks LVGL panics or WiFi/HTTP failure modes that look like network bugs.

---

## Prior firmware: `sensor_example/`

`sensor_example/` holds a **frozen copy of the previous firmware** that the BOX-3 ran before this project pivoted to monitoring Beszel. It boots a six-tab LVGL dashboard that exercises every peripheral on the ESP32-S3-BOX-3 + BOX-3-SENSOR extension board:

| Tab | What it shows |
|-----|---------------|
| **IMU** | Live accel / gyro / tilt from the on-board ICM42670 |
| **Env** | Temperature / humidity from the AHT30 on the SENSOR extension |
| **Radar** | AT581X presence detection events + count |
| **Audio** | ES7210 mic RMS bar + "Beep" button (ES8311 speaker, 1 kHz tone) |
| **IR** | RX pulse count + "Send Test" NEC frame over the IR diodes |
| **Btn** | Short / long press counters for CONFIG, MUTE, MAIN buttons |

It existed to **verify each peripheral worked** before any application code was written. Now that those peripherals are confirmed and the project's purpose has narrowed to the Beszel dashboard, the self-test source lives in `sensor_example/` as documentation: copy it back into `main/` and rebuild if you ever need to re-validate the board or port an individual sensor driver into a new project.

The folder is **not compiled by the top-level `CMakeLists.txt`** — it is reference material only. To rebuild and flash the old self-test, temporarily point `main/`'s CMake at `sensor_example/` (or copy its files into `main/`) and run the standard build/flash flow.

---

## Common pitfalls

These cost real time and are documented in detail with file/line references in [LearnedPatterns.md](LearnedPatterns.md):

- **`json` component is missing in ESP-IDF v6.x** — cJSON is now the standalone managed component `espressif/cjson`. The legacy `REQUIRES json` line fails to resolve. Declare `espressif/cjson` in [main/idf_component.yml](main/idf_component.yml).
- **`NAME_MAX` collides with picolibc's filesystem constant** (255). The xtensa-esp-elf toolchain pulls `<sys/syslimits.h>` transitively through BSP / FreeRTOS headers — never `#define NAME_MAX` in your own code. Rename to e.g. `HOST_NAME_MAX_LEN`.
- **`printf("%u", uint32_t)` is `-Werror=format=` under picolibc** — on the xtensa target, `uint32_t = unsigned long`, not `unsigned int`. Cast to `(unsigned)` or use `PRIu32` from `<inttypes.h>`.
- **Beszel `info.g` (GPU usage) is `omitempty`** — when current GPU usage is exactly 0 %, the JSON field is dropped entirely. A single snapshot of `/api/collections/systems/records` cannot distinguish "host has no GPU" from "host's GPU is idle". This firmware sidesteps the ambiguity by always rendering the GPU bar and defaulting to 0 % when the field is absent.
- **`idf.py flash` cannot find the chip** — usually the USB-C cable is power-only, or Zadig drivers are swapped.
- **`sdkconfig` overrides `sdkconfig.defaults`** once it exists. If you flip a Kconfig value in `sdkconfig.defaults` but the build still uses the old value, the answer is in `sdkconfig` — either patch it there too, or delete it and `idf.py reconfigure`.

---

## Adding your own metric / endpoint

1. Read [CLAUDE.md](CLAUDE.md) §2 (style) and §7 (research-before-coding).
2. If the metric comes from a new Beszel field, look at the **raw JSON dump** that prints once on first boot (`I beszel: raw systems response (...)`). It shows exactly what keys Beszel is sending for your host.
3. Add the new key to `cpu_keys[]` / `mem_keys[]` / `gpu_keys[]` (or create a new key list) in [parse_one_system in main/beszel.c](main/beszel.c).
4. Extend [`ui_beszel_host_t` in main/ui.h](main/ui.h) and the per-tab widgets in [build_host_tab in main/ui.c](main/ui.c) (every LVGL call from outside the LVGL task **must** be wrapped in `bsp_display_lock` / `bsp_display_unlock` — the `UI_WITH_LOCK` macro handles this).
5. Append a dated section to [ToDo.md](ToDo.md), then check items off as you go.
6. When the work is done, distill any new gotcha into [LearnedPatterns.md](LearnedPatterns.md).

---

## License

See per-component licenses under `managed_components/`. Application source under `main/` and `sensor_example/` is unencumbered — use as a reference for your own BOX-3 projects.
