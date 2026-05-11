# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP-IDF firmware for the **ESP32-S3-BOX-3** dev board. Reads the on-board ICM42670 6-axis IMU over I2C and displays accel/gyro/tilt on the 320×240 LCD via LVGL.

- Language: C (no C++)
- Framework: ESP-IDF (≥5.3; user is on v6.0.1)
- Target chip: `esp32s3` (fixed in `sdkconfig.defaults`)
- CMake project name: `my_box3_sensor`
- Source: single TU at `main/main.c`

## Common commands

Run from the project root. Assumes ESP-IDF export script has been sourced (`$env:IDF_PATH` set, `idf.py` on PATH).

```powershell
idf.py set-target esp32s3        # one-time, only if build/ is missing or target changed
idf.py build                      # full build
idf.py -p COM<N> flash monitor    # flash + open serial (Ctrl+] to exit)
idf.py monitor                    # serial monitor only
idf.py menuconfig                 # change sdkconfig (LVGL fonts, log levels, etc.)
idf.py fullclean                  # nuke build/ when CMake gets confused
idf.py reconfigure                # re-run CMake without wiping build/
```

Component lock-in lives in `dependencies.lock`. Editing `main/idf_component.yml` requires `idf.py reconfigure` (or just `build`) to refetch.

## Architecture

### Initialization order (must not be reordered)

In `app_main()` ([main/main.c](main/main.c)):

1. `bsp_i2c_init()` — brings up the shared I2C bus that both touch and IMU live on
2. `bsp_display_start()` — sets up LCD panel + LVGL + touch input device, returns an `lv_display_t *`
3. `bsp_display_backlight_on()` — display is dark until this is called
4. `imu_init()` — calls `bsp_i2c_get_handle()` (returns the handle, does **not** take `&out`) and passes it to `icm42670_create(bus, addr, &handle)` (3 args, not a config struct)
5. UI creation must be wrapped in `bsp_display_lock(0)` / `bsp_display_unlock()`
6. `xTaskCreatePinnedToCore` pins the sensor loop to core 1 so it doesn't fight LVGL on core 0

### LVGL threading rule

Every `lv_*` call from outside the LVGL task **must** be guarded by `bsp_display_lock(timeout_ms)` / `bsp_display_unlock()`. The sensor task in `main.c` follows this pattern — preserve it in any new code that touches widgets.

### Fonts

Only `lv_font_montserrat_14` is enabled by default. Other sizes (`_12`, `_18`, etc.) cause undeclared-identifier errors unless turned on via `idf.py menuconfig` → *Component config → LVGL configuration → Font usage*.

## Hardware-specific sdkconfig

`sdkconfig.defaults` encodes BOX-3-specific board config — don't change these without a reason:

- `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` + `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE` — 16 MB flash, single-app partition (no OTA)
- `CONFIG_SPIRAM_MODE_OCT` + `CONFIG_SPIRAM_SPEED_80M` — Octal PSRAM at 80 MHz (BOX-3 specifically, not BOX or BOX-Lite)
- `CONFIG_SPIRAM_USE_MALLOC` — `malloc()` can return PSRAM addresses; required for LVGL double buffers
- `CONFIG_LV_MEM_CUSTOM` — LVGL uses ESP-IDF heap rather than its own pool
- `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` — console is on the USB-C port's built-in JTAG, **not** UART0

If `idf.py flash` can't find the chip, the cable is likely power-only; the board needs the USB-C data port and the JTAG console enabled (above).

## Dependencies

Declared in `main/idf_component.yml`:

- `espressif/esp-box-3` — BSP (display, touch, I2C, audio, buttons). Owns the LVGL task.
- `espressif/icm42670` — IMU driver. API takes the I2C bus handle directly; there is no `i2c_handle` / `i2c_addr` field in `icm42670_cfg_t` (that struct only holds `acce_fs/odr` + `gyro_fs/odr`).

Pulled transitively (visible in `managed_components/`): LVGL 9.x, `esp_lvgl_port`, touch drivers (gt911 + tt21100 — BOX-3 ships with one or the other depending on revision), `esp_lcd_ili9341`, etc. Don't edit anything under `managed_components/` — it's regenerated from `dependencies.lock`.

---

# CommonClaude Conventions

The rules below are adopted from [coport-uni/CommonClaude](https://github.com/coport-uni/CommonClaude), adapted for this C/ESP-IDF project. They sit on top of the project-specific guidance above. When rules conflict, the more specific one wins (project section > CommonClaude section).

## 1. Rule Priority

Project-level `CLAUDE.md` files supersede global rulesets. When conflicts arise, more-specific context wins. The project-specific sections at the top of this file override the CommonClaude defaults below.

---

## 2. C Code Convention (Google style guide, C-adapted)

All new C code follows the C-applicable subset of the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). The official ESP-IDF code in `managed_components/` is third-party and is not held to these rules.

### Naming

| Element | Style | Example |
|---------|-------|---------|
| Variable / parameter / local | `snake_case` | `accel_x`, `i2c_handle` |
| Function | `snake_case` (often `module_action` form) | `imu_init`, `bsp_display_lock` |
| Struct / typedef | `snake_case_t` (ESP-IDF convention) | `icm42670_cfg_t` |
| Enum constant | `SCREAMING_SNAKE_CASE` | `ACCE_FS_4G` |
| Macro / `#define` constant | `SCREAMING_SNAKE_CASE` | `ICM42670_I2C_ADDRESS` |
| File | `snake_case.c` / `snake_case.h` | `main.c`, `sensor_task.h` |
| Module-internal globals | `static` + `snake_case` (prefix `s_` optional) | `static icm42670_handle_t imu` |

Additional rules:
- Names must be pronounceable; avoid abbreviations unless industry-standard (`i2c`, `gpio`, `dma`).
- Name length scales with scope — short for tight loops, descriptive for public APIs.
- Variables and structs are nouns; functions are verbs (`read_*`, `init_*`, `set_*`).

### Structure

- **80-column line limit** for new code.
- One statement per line; no comma operator chains.
- Indent with **4 spaces** (never tabs). Existing project files already use 4 spaces.
- Declare variables in the narrowest possible scope, close to first use.
- Use file-scope `static` for module-internal globals and helpers (no exported symbols without intent).

### Header files

- One header per public module, included via `#include "module.h"`.
- Use header guards: `#pragma once` (already standard across ESP-IDF) or the `PROJECT_PATH_FILE_H_` form.
- Include order, separated by blank lines:
  1. The matching `.h` for a `.c` file
  2. C system headers (`<stdint.h>`, `<string.h>`, …)
  3. ESP-IDF / FreeRTOS headers (`"freertos/FreeRTOS.h"`, `"esp_log.h"`, …)
  4. Third-party / managed component headers (`"lvgl.h"`, `"bsp/esp-box-3.h"`, `"icm42670.h"`)
  5. Project-internal headers
- Prefer forward declarations of structs over `#include` when only a pointer is used.

### Spacing & braces

- One space after commas, none before: `foo(a, b, c)`.
- One space around binary operators (`=`, `==`, `+`, `&&`, …).
- K&R brace style: opening brace on the same line for `if`/`for`/`while`; on its own line for function definitions (already used in `main.c`).
- Always brace single-statement bodies — no brace-less `if (x) do_y();`.

### Comments

- Use complete sentences; comment the *why*, never restate the *what*.
- Outdated comments must be deleted, not preserved.
- TODO format with owner attribution:
  ```c
  // TODO(@owner): brief description of work needed.
  // Additional context on why this matters.
  ```
- Public functions declared in headers get a short Doxygen-style block describing purpose, params, and return value (matches existing managed component style).

### Language

All code comments, commit messages, and PR descriptions must be in **English**. User-facing strings (logs, UI labels) may be Korean if appropriate to the application.

---

## 3. Debug File Management

Debug, exploratory, and throwaway scratch files go in `claude_test/`, **not** mixed into `main/` or pushed into a test directory.

| Location | Contents |
|----------|----------|
| `main/`, `components/` | Production firmware code |
| `claude_test/` | One-off probes, register dump scripts, scratch ESP-IDF examples, host-side Python diagnostics |

### When writing debug code
1. Create files directly under `claude_test/` (e.g., `claude_test/probe_icm42670_whoami.c`).
2. Add a one-line comment at the top of the file explaining intent and expected lifetime.
3. If the debug script produces a real fix, fold the relevant logic into `main/` or a proper component and delete the debug file.

### Index
`claude_test/README.md` is the index. When adding a debug file, append a row describing purpose and lessons learned.

---

## 4. Task Management

**Mandatory** workflow for every task, regardless of size:

1. **Validate the command** before doing anything else (see "Command Input Validation" below).
2. **Write or append to `ToDo.md`** at the project repo root (`Espress_dev/ToDo.md`, alongside this `CLAUDE.md`) in this format:
   ```markdown
   ## YYYY-MM-DD | Task title

   - [ ] subtask 1
   - [ ] subtask 2
   ```
   Never delete or rewrite previous entries — `ToDo.md` is append-only history.
3. **Get user confirmation** on the `ToDo.md` contents before starting work.
4. **Create a GitHub issue** via `gh issue create` when a remote repo exists. (Skip silently if `gh` is unavailable or no remote is configured.)
5. **Check items off** (`- [ ]` → `- [x]`) as work completes; append a one-line summary or output path after the checkbox.
6. **Commit and push** after each user command completes (only when working in a git repo).

### Command Input Validation

Before writing `ToDo.md`, confirm both:
- **Is the command explicit?** If ambiguous, ask: *what* changes (target), *how* (method), *why* (purpose).
- **Are reference materials available?** Check for related datasheets, PDFs, prior code, or vendor docs and review them first.

If either check fails, stop and ask the user.

### Append-only exceptions

The append-only rule does **not** forbid:
- Flipping `- [ ]` to `- [x]` and appending an output path or commit hash.
- Adding a new dated section at the bottom.

It does forbid: rewriting past task bodies, reordering items, deleting old sections.

---

## 5. Testing Rules

When tests exist, code quality must never be sacrificed to make them pass.

1. **No magic numbers.** Use named constants (`#define`, `static const`, or `enum`) with meaningful names. `M_PI`, `ICM42670_I2C_ADDRESS` — good. A bare `0x68` in driver code — not good.
2. **No hardcoding to match test inputs.** Fix the logic, not the branch that the test happens to exercise.
3. **Code quality first.** Readability and correctness beat green CI on tricked code.

---

## 6. Build & Static Checks

There is no Ruff equivalent for ESP-IDF C. Treat the compiler as the linter.

1. **Zero warnings.** `idf.py build` must complete with no warnings on touched files. Common offenders to fix at the source: `-Wint-conversion`, `-Wunused-variable`, `-Wimplicit-function-declaration`, `-Wincompatible-pointer-types`.
2. **Verify on real hardware after non-trivial changes.** A clean build is necessary but not sufficient — at minimum, flash and confirm boot logs and that the failing-prone subsystem (LVGL, IMU, I2C) initializes.
3. **Never silence warnings** with casts or `#pragma` unless the cast is genuinely correct and the reason is documented in a comment.

### Automated hooks (`.claude/`)

This project enforces parts of the rules above via Claude Code hooks declared in [.claude/settings.json](.claude/settings.json). The hooks are PowerShell scripts under [.claude/hooks/](.claude/hooks/):

| Hook | Event | Behavior |
|------|-------|----------|
| `pre-write-guard.ps1`        | `PreToolUse` Write/Edit  | Blocks edits to `managed_components/` or `build/`. Blocks files with `debug_`/`scratch_`/`tmp_`/`probe_`/`experiment_` prefixes anywhere outside `claude_test/`. |
| `post-write-debug-remind.ps1`| `PostToolUse` Write/Edit | Reminds to update `claude_test/README.md` after any file is added under `claude_test/`. |
| `post-write-build-check.ps1` | `PostToolUse` Write/Edit | After touching `main/**/*.c|*.h`, `CMakeLists.txt`, `sdkconfig.defaults`, or `idf_component.yml`, runs `idf.py build` (up to 180s). Surfaces compile errors and warnings to the agent. Skips silently if `idf.py` is not on `PATH`. Combined log is written to `.claude/last-build.log`. |

To disable a hook temporarily, comment it out in `.claude/settings.json`. To run the build check manually instead of on every save, remove its entry from `PostToolUse` and rely on `idf.py build` from the terminal.

---

## 7. Research Before Coding

Before calling an unfamiliar API, header, or component, verify its actual interface rather than guessing from memory.

1. **Consult official documentation first** — the component README under `managed_components/<name>/README.md`, the ESP-IDF API reference, or the upstream project page.
2. **Read the header.** For any function from `managed_components/`, open the matching `.h` and confirm the real signature before calling it. This project has already produced bugs from assuming `bsp_i2c_get_handle(&h)` and `icm42670_create(&cfg, &h)` — both wrong.
3. **Search the codebase** for prior usage of the same symbol before writing new code against it.
4. **Trust documentation over intuition.** When docs disagree with the mental model, update the mental model.

---

## 8. Exceptions

The rules above apply to production code under `main/` and `components/`. The following contexts receive formal waivers:

### `claude_test/` scratch files
Exempt from:
- The 80-column line limit (§2 Structure).
- Comprehensive Doxygen blocks on public functions (§2 Comments).
- The "no magic numbers" rule (§5), provided the file opens with an intent comment naming the purpose and expected lifetime.

Code promoted into `main/` or a real component must conform fully.

### `ToDo.md` checkbox updates
Marking completion (flipping `- [ ]` to `- [x]`, appending output path/commit hash) is permitted under the append-only rule. Prose rewrites, reordering, and deletion are not.

---

## 9. Learned Patterns Reference

When `LearnedPatterns.md` exists at the project repo root (`Espress_dev/LearnedPatterns.md`, alongside this `CLAUDE.md`), treat it as part of the workflow.

1. **Before drafting `ToDo.md`**, read the sections of `LearnedPatterns.md` relevant to the new task (filter by component, library, or hardware area).
2. **Reference applicable patterns** in ToDo entries using `(see LP §X)` where `X` is the section number:
   ```
   - [ ] Add tap-detection on ICM42670 (see LP §3)
   ```
3. **After task completion**, append any new gotcha, library quirk, environment note, or workflow lesson to the matching section of `LearnedPatterns.md` using the Problem / Cause / Fix / Rule format from §10.
4. **Promote stable patterns** that recur across many tasks into this `CLAUDE.md` and remove them from `LearnedPatterns.md` to avoid duplication.

---

## 10. Learned Patterns Bootstrap

If `LearnedPatterns.md` does not exist, generate it by analyzing completed (`[x]`) items in `ToDo.md`.

### Procedure
1. Read every `[x]` item across all sections of `ToDo.md`.
2. Classify each into exactly one of:
   - **§1. Recurring Issues** — the same or similar problem appeared **two or more times**
   - **§2. Solved Gotchas** — one-time trap with credible chance of recurring
   - **§3. Library Quirks** — hidden or surprising behavior of a specific library, BSP, or driver (e.g., `bsp_i2c_get_handle` returning rather than taking `&out`)
   - **§4. Workflow Lessons** — process or collaboration lessons
   - **§5. Environment Specifics** — Windows / ESP-IDF / PowerShell / USB-Serial JTAG / BOX-3 hardware quirks
   - **§99. Uncategorized** — anything that doesn't fit; do not discard
3. For each entry, record four single lines:
   - **Problem**: what went wrong
   - **Cause**: the underlying reason
   - **Fix**: the specific change that resolved it
   - **Rule**: a short directive in `Always …` / `Never …` form
4. Append `(from ToDo: YYYY-MM-DD task name)` for traceability.

### Constraints
- **Do not modify `ToDo.md`.** It is append-only; edits happen only in `LearnedPatterns.md`.
- **Create `LearnedPatterns.md` as a new file** at the project repo root (alongside this `CLAUDE.md`).
- **Do not invent patterns.** When a ToDo item is ambiguous, place it under §99 rather than guessing.
- **Write all content in English**, consistent with §2 Language.
