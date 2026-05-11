# LearnedPatterns.md

Lessons distilled from completed (`[x]`) items in [ToDo.md](ToDo.md). Read the relevant section before drafting new tasks; append new findings here after each task. Each entry follows the Problem / Cause / Fix / Rule format, with `(from ToDo: ...)` traceability at the end.

Created: 2026-05-11 (bootstrap from BOX-3 firmware work)

---

## §1. Recurring Issues

(none yet — promote here once the same problem class appears twice or more)

---

## §2. Solved Gotchas

### 2.1 ESP-IDF `sdkconfig` overrides `sdkconfig.defaults` once it exists

- **Problem**: Adding `CONFIG_LV_USE_FLOAT=y` to `sdkconfig.defaults` had no effect on the next build; LVGL still printed only the literal `f`.
- **Cause**: `sdkconfig.defaults` is consulted only when a key is *absent* from `sdkconfig`. After the first build, `sdkconfig` already contained `# CONFIG_LV_USE_FLOAT is not set` (explicit `n`), which won over the new default.
- **Fix**: Edit both files — keep the value in `sdkconfig.defaults` for reproducibility, AND replace the existing line in `sdkconfig`. Alternatively delete `sdkconfig` and let `idf.py reconfigure` regenerate it.
- **Rule**: When changing an existing Kconfig value, never assume `sdkconfig.defaults` alone is enough — patch `sdkconfig` too, or remove it. (from ToDo: 2026-05-11 Accel/Gyro "f" 표시 + AHT30 미동작 진단)

### 2.2 Boot log capture must cover the first sensor polling cycle

- **Problem**: Initial monitor log was truncated at `main_task: Returned from app_main()` (~t=1558ms), making it impossible to tell whether AHT30 polling was succeeding or timing out.
- **Cause**: First sensor_hub polling fires at `iot_sensor_start + min_delay` (= ~t=2418ms for 1 s period), one second *after* `app_main()` returns.
- **Fix**: Always capture at least 5–10 s of serial output for sensor diagnostics, covering multiple polling cycles.
- **Rule**: For sensor_hub / sensor-polling debugging, capture monitor output for `min_delay × 3` minimum, never just the boot banner. (from ToDo: 2026-05-11 마이크/AHT30 추가 진단)

---

## §3. Library Quirks

### 3.1 LVGL built-in `sprintf` drops `%f` unless `LV_USE_FLOAT=y`

- **Problem**: `lv_label_set_text_fmt(lbl, "X: %+.2f", ax)` produced `X: f` instead of the numeric value.
- **Cause**: LVGL's builtin printf defines `PRINTF_DISABLE_SUPPORT_FLOAT = !LV_USE_FLOAT`. When `LV_USE_FLOAT=n` (default), the entire `case 'f':` branch is `#if`-compiled out (managed_components/lvgl__lvgl/src/stdlib/builtin/lv_sprintf_builtin.c:42, 776-794). The format scanner consumes flags/precision then emits the unrecognized specifier literally.
- **Fix**: Enable `CONFIG_LV_USE_FLOAT=y` in `sdkconfig.defaults`. Alternative: use stdio `snprintf` into a buffer then `lv_label_set_text`.
- **Rule**: Any `lv_label_set_text_fmt` call with `%f`/`%e`/`%g` on this project assumes `LV_USE_FLOAT=y`. If float printing breaks in the future, check Kconfig first. (from ToDo: 2026-05-11 Accel/Gyro "f" 표시 + AHT30 미동작 진단)

### 3.2 BOX-3 BSP routes `HUMITURE_ID` to the dock I2C bus, not main

- **Problem**: AHT30 driver was created and `iot_sensor_start` returned OK, but no temperature/humidity events ever fired.
- **Cause**: `bsp_sensor_init(HUMITURE_ID, ...)` internally uses `i2c_dock_handle` (managed_components/espressif__esp-box-3/esp-box-3.c:890-893), which lives on GPIO 40/41 — wired to the PMOD1 connector, not the main I2C bus. With nothing on PMOD1, the sensor cannot respond no matter how correct the code is.
- **Fix**: Plug in the official ESP32-S3-BOX-3-SENSOR extension board (or any board with AHT30 + pull-ups on PMOD1).
- **Rule**: BOX-3 humiture / radar / IR sensors all live on PMOD1 (dock I2C), never on the main I2C bus. Verify physical extension presence before debugging sensor_hub events. (from ToDo: 2026-05-11 Accel/Gyro "f" 표시 + AHT30 미동작 진단)

### 3.3 BOX-3 BSP audio: gain must be set before `esp_codec_dev_open`

- **Problem**: ES7210 mic RMS bar stayed near zero even after `bsp_audio_codec_microphone_init` succeeded.
- **Cause**: `esp_codec_dev_set_in_gain` was called *after* `esp_codec_dev_open`, which differs from the BSP `API.md` reference example (gain 42.0 set before open).
- **Fix**: Match the BSP convention — call `esp_codec_dev_set_in_gain(mic, 42.0f)` first, then `esp_codec_dev_open`.
- **Rule**: Follow the BSP API.md sample order verbatim for ES7210/ES8311 init — gain before open, codec_dev created before sample_info struct passed in. (from ToDo: 2026-05-11 마이크/AHT30 추가 진단)

### 3.4 `sensor_hub`: `iot_sensor_handler_register_with_type` posts events to a different base than the polling task

- **Problem**: AHT30 humiture display never updated. Boot log showed `SENSOR_HUB: Sensor created ...` and `SENSOR_LOOP: register a new handler to event loop succeed × 2`, no `AHT30:` errors. Callback `humiture_event_cb` fired zero times.
- **Cause**: `sensor_hub` exposes two registration APIs that compile and register cleanly but bind to **different `esp_event` bases**. The polling task posts to a per-instance dynamic base built at `iot_sensor_hub.c:359` as `sprintf(sensor->event_base, "%s_%x", sensor_name, addr)` (e.g. `"sensor_hub_aht30_38"`) — that's what `sensors_event_post` uses at line 251. `iot_sensor_handler_register(handle, cb, ctx)` registers on `sensor->event_base` (line 634) → matches polling output. `iot_sensor_handler_register_with_type(HUMITURE_ID, event_id, cb, ctx)` registers on the **fixed macro base** `SENSOR_HUMITURE_EVENTS` (line 658) → never receives polling output. esp_event silently drops the mismatch.
- **Fix**: Use the handle-based variant: `iot_sensor_handler_register(s_humiture, humiture_event_cb, NULL)`. Branch on `event_id` inside the callback if you need per-event-id behaviour — `ESP_EVENT_ANY_ID` delivers everything (TEMP / HUMI / STARTED / STOPPED).
- **Rule**: When sensor_hub returns a handle, register against the handle. Treat `_with_type` as broken-by-design for the polling path; reach for it only if you also forward events from `sensor->event_base` to the macro base yourself. (from ToDo: 2026-05-11 AHT30 silent event drop 수정)

### 3.5 `esp_codec_dev_read`/`esp_codec_dev_write` return 0 on success, not the byte count

- **Problem**: ES7210 mic RMS bar stayed at 0 even though boot log showed `ES7210: Unmuted` and `Adev_Codec: Open codec device OK`. Beep button always set status to `beep write fail` even when sound played correctly.
- **Cause**: These APIs do **not** follow the POSIX `read`/`write` convention. Return value is `ESP_CODEC_DEV_OK` (= 0) on success, negative `ESP_CODEC_DEV_*` codes on failure (managed_components/espressif__esp_codec_dev/platform/audio_codec_data_i2s.c:717). The internal `bytes_read` / `bytes_written` counters are discarded inside the wrapper. User code that checks `if (got > 0)` or `if (written <= 0)` treats every successful call as a failure, breaking the data path silently.
- **Fix**: Check `== ESP_CODEC_DEV_OK` (or `== 0`) for success, and trust that the buffer was filled with exactly the requested `len` bytes (the wrapper blocks on `i2s_channel_read` with `DEFAULT_WAIT_TIMEOUT` until full).
- **Rule**: For any `esp_codec_dev_*` API, look up the return convention in `esp_codec_dev_types.h` before writing the success check. Don't reuse POSIX `read`/`write` muscle memory. (from ToDo: 2026-05-11 마이크 RMS 0 + beep 항상 실패 수정)

### 3.6 `bsp_i2c_get_handle()` returns the handle; does not take `&out`

- **Problem**: Earlier draft tried `bsp_i2c_get_handle(&bus)` and `icm42670_create(&cfg, &handle)`, both of which fail to compile or link.
- **Cause**: The ESP-BOX-3 BSP exposes the I2C bus via a value-returning getter, and `icm42670_create` takes three positional args `(bus, addr, &handle_out)` — there is no config-struct overload. This is documented in the project CLAUDE.md but easy to forget.
- **Fix**: Use `i2c_master_bus_handle_t bus = bsp_i2c_get_handle();` then `icm42670_create(bus, ICM42670_I2C_ADDRESS, &handle)`.
- **Rule**: Before calling any `managed_components/` function, open its `.h` and confirm the real signature (project CLAUDE.md §7 Research Before Coding). Memory of similar APIs is unreliable. (from CLAUDE.md prior incident; reinforced 2026-05-11)

---

## §4. Workflow Lessons

### 4.1 Diagnose float / printf bugs at the formatter level, not the data source

- **Problem**: Initially suspected the IMU driver was returning wrong values when Accel/Gyro showed only `f`.
- **Cause**: Skipped checking how the format spec was rendered. The IMU was fine; LVGL's sprintf was the actual failure point.
- **Fix**: When the suspicious value matches a literal piece of the format string ("f", "d", "%"), check the formatter implementation before the data path.
- **Rule**: A printed character that looks like a format specifier means the formatter dropped the conversion. Audit the printf implementation before the data source. (from ToDo: 2026-05-11 Accel/Gyro "f" 표시 + AHT30 미동작 진단)

---

## §5. Environment Specifics

### 5.1 BOX-3 PMOD1 (GPIO 40/41) has no on-board I²C pull-ups

- **Problem**: The dock I²C bus relies on external pull-ups, but the BOX-3 main board does not populate them.
- **Cause**: BSP header explicitly states "Intended for I2C SCL/SDA (pull-up NOT populated)" (managed_components/espressif__esp-box-3/include/bsp/esp-box-3.h:167,171). Espressif assumed users would plug a board (BOX-3-SENSOR or custom) that supplies the pull-ups.
- **Fix**: Use the official ESP32-S3-BOX-3-SENSOR extension, or add external ~4.7 kΩ pull-ups to 3V3 on SDA/SCL when prototyping with a bare PMOD board.
- **Rule**: Before debugging dock-I²C devices (AHT30, AT581X, …) in software, confirm a pull-up source is physically present. (from ToDo: 2026-05-11 ESP32-S3-BOX-3 + SENSOR 액세서리 전체 기능 점검)

### 5.2 `idf.py` is not on the host PATH from the Claude Code shell

- **Problem**: Project hook `.claude/hooks/post-write-build-check.ps1` is supposed to run `idf.py build` after edits, and Claude cannot run it manually either — the agent cannot verify builds without user assistance.
- **Cause**: ESP-IDF is installed at `C:\Espressif\tools\python\v6.0.1\venv\` and exposed only via the Espressif export script, which the Claude session does not source.
- **Fix**: Edits that touch `main/**`, `sdkconfig*`, or `idf_component.yml` must be followed by the user running `idf.py build && idf.py -p COM13 flash monitor` in their IDF shell and sharing the result.
- **Rule**: After any source/config change, surface to the user that a manual `idf.py build && flash && monitor` cycle is required before treating the task as verified. (from ToDo: 2026-05-11 UI 흰색 → 검정, 2026-05-11 Accel/Gyro 진단, 2026-05-11 마이크/AHT30 진단)

### 5.3 BOX-3 USB-C data port + USB-Serial JTAG console required for flash

- **Problem**: If `idf.py flash` cannot find the chip, the most common cause is the cable or console assignment.
- **Cause**: BOX-3 console is on the USB-C built-in JTAG (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`), not UART0. A power-only USB-C cable will not enumerate.
- **Fix**: Use a data-capable USB-C cable; keep `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in `sdkconfig.defaults`.
- **Rule**: Document this in `sdkconfig.defaults` comments and check it first when flash fails. (from CLAUDE.md "Hardware-specific sdkconfig"; reinforced by 2026-05-11 monitor session at COM13)

### 5.4 Windows Zadig: assign one interface CDC, the other WinUSB v6

- **Problem**: On a fresh Windows host, the ESP32-S3 USB-Serial-JTAG enumerates as two interfaces but only one of `idf.py monitor` (serial) and `idf.py openocd`/JTAG flash works at a time — or neither, depending on which driver Windows auto-binds.
- **Cause**: The S3 exposes a composite USB device — interface 0 is CDC ACM (virtual COM port for `idf.py monitor`) and interface 1 is the JTAG endpoint (libusb-style, needs WinUSB). Windows does not always assign the right driver to each interface, and forcing WinUSB on the CDC half kills the COM port (or vice versa).
- **Fix**: Run [Zadig](https://zadig.akeo.ie/) → `Options → List All Devices` → for the **CDC interface** keep/select the USB Serial (CDC) driver, for the **JTAG interface** install **WinUSB v6.x**. Replace, never globally swap.
- **Rule**: After plugging a new BOX-3 (or a new Windows host), verify in Device Manager that the CDC half shows a COM port and the JTAG half shows `WinUSB` — do not run Zadig "Replace Driver" against the wrong interface. (from ToDo: 2026-05-11 ToDo/LP repo-local 전환 + 환경 LP 추가)

### 5.5 Use the ESP-IDF VS Code extension instead of a separate IDF shell

- **Problem**: `idf.py` is not on the Claude session's `PATH` (LP §5.2) and opening a separate IDF PowerShell window for every build/flash/monitor is friction-heavy.
- **Cause**: The export script bound to `C:\Espressif\tools\python\v6.0.1\venv\` is what `idf.py` depends on; only the Espressif IDF VS Code extension (`espressif.esp-idf-extension`) integrates that setup transparently.
- **Fix**: Install the **ESP-IDF VS Code extension** (already configured in [.vscode/settings.json](.vscode/settings.json) with `idf.currentSetup`). Drive build/flash/monitor with its chord shortcuts:
  - `Ctrl+E B` — Build
  - `Ctrl+E F` — Flash
  - `Ctrl+E M` — Monitor (Ctrl+] to exit)
  - `Ctrl+E D` — Build + Flash + Monitor in one shot
  - `Ctrl+E P` — Select serial port (also persisted as `idf.portWin` in settings.json)
  - `Ctrl+E T` — Set target
  - `Ctrl+E G` — GUI menuconfig
  - `Ctrl+Shift+P` → `ESP-IDF: …` — full command palette
- **Rule**: Prefer the VS Code extension chord shortcuts over a separate `idf.py` shell for routine build/flash/monitor. Drop to a terminal only for commands the extension does not surface (e.g. `idf.py fullclean`). (from ToDo: 2026-05-11 ToDo/LP repo-local 전환 + 환경 LP 추가)

---

## §99. Uncategorized

(empty — temporary holding spot for findings that do not fit §1-§5)
