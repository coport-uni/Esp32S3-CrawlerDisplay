## 2026-05-11 | ESP32-S3-BOX-3 + SENSOR 액세서리 전체 기능 점검 코드

작업 경로: `container/Espress_dev/`. SENSOR 액세서리(레이더·온습도·IR)까지 포함한 LVGL 대시보드 펌웨어 작성.

### 사전 조사 결과 (Research Before Coding)

- 레이더 칩셋: **AT581X** (Airoha, 5.8 GHz). HLK-LD2410 아님. Espressif 공식 factory_demo와 일치.
- 레이더 인터페이스: dock I2C 버스 (GPIO 40 SCL / 41 SDA), I2C addr 0x28, INT 핀 **GPIO 21**.
- 온습도: **AHT30**. BSP `bsp_sensor_init(HUMITURE_ID, ...)`로 직접 지원 (managed_components에 이미 포함).
- IR 핀: BOX-3-SENSOR 공식 schematic 미공개. 잠정값 TX=GPIO 39 (PMOD1_IO3), RX=GPIO 38 (PMOD1_IO7) — `#define`으로 노출시켜 사용자 검증 후 조정 가능하게 한다.
- 추가 의존성: `espressif/at581x: ^0.1.0` 만 추가 (IR은 ESP-IDF 내장 RMT 드라이버로 처리).

### 작업 항목

- [ ] `main/idf_component.yml`에 `espressif/at581x` 의존성 추가
- [ ] LVGL UI를 tabview 구조로 재구성 (`main/ui.c/h`): IMU / 환경 / 레이더 / 오디오 / IR / 버튼 6개 탭
- [ ] IMU 탭: 기존 가속도·자이로·tilt 표시 유지
- [ ] 환경 탭(AHT30): `bsp_sensor_init(HUMITURE_ID)` 후 1초 주기 온도·습도 표시
- [ ] 레이더 탭(AT581X): dock I2C 버스에 디바이스 생성, INT GPIO 21 ISR 콜백에서 LVGL 락 잡고 상태 라벨 갱신, 마지막 감지 timestamp 표시
- [ ] 오디오 탭: ES7210 마이크에서 일정 샘플 받아 RMS 계산 → 바 표시, "Beep" 버튼 누르면 ES8311 스피커로 1 kHz 짧은 사인 톤 출력
- [ ] IR 탭: RX RMT 채널 + TX RMT 채널 초기화, 수신 시 펄스 개수 표시, "Send NEC" 버튼으로 임의 NEC 코드 송신
- [ ] 버튼 탭: `bsp_iot_button_create()`로 CONFIG/MUTE/MAIN 3개 버튼 핸들 받고 short/long press 카운터 표시
- [ ] 탭 전환은 화면 좌우 스와이프 + 상단 헤더 탭 클릭 둘 다 지원
- [ ] 초기화 순서 검증: I2C → display → backlight → 모든 센서/오디오 → LVGL UI(락) → 태스크 → ISR
- [ ] `idf.py build` 통과 (warning 0, [.claude/hooks/post-write-build-check.ps1](.claude/hooks/post-write-build-check.ps1) 사용)
- [ ] 실기기 flash 후 6개 탭 모두 동작 확인 (LP §3 기록용)
- [ ] LearnedPatterns.md에 BOX-3-SENSOR 핀 매핑 / AT581X 사용법 추가

### 불확실 항목 (사용자 결정 필요)

- IR 핀: BOX-3-SENSOR 모듈의 실제 schematic 확보 가능한지? 불가하면 GPIO 38/39 잠정값으로 시작하고 실기기 테스트로 보정
- AT581X INT의 active 레벨: 데이터시트 기준 active-high 가정 (코드에서 `interrupt_level=1`). 동작 안 하면 0으로 토글

## 2026-05-11 | UI 흰색 글씨 → 검정으로 변경 (Espress_dev)

- [x] `main/ui.c`의 `make_value_label()`에서 `lv_color_white()` → `lv_color_black()` 변경 (main/ui.c:55)
- [x] 색상 라벨 (COLOR_ACCENT/WARN/OK/MUTED/PINK)은 유지
- [x] 배경(COLOR_BG)도 유지 (사용자 결정)
- [ ] `idf.py build` 통과 확인 — 현재 셸에 idf.py 미등록, 사용자 IDF 환경에서 수동 빌드 필요

## 2026-05-11 | Accel/Gyro "f" 표시 + AHT30 미동작 진단 (Espress_dev)

증상: IMU 탭의 Accel/Gyro 값이 "X: f", "Y: f"처럼 부동소수 자리에 'f'만 출력. Env 탭의 AHT30(온/습도)는 갱신 자체가 없음. Dock은 연결돼 있음.

- [x] 원인 분석: LVGL 내장 sprintf가 `LV_USE_FLOAT=n`일 때 `%f` case 자체가 컴파일에서 빠짐 (managed_components/lvgl__lvgl/src/stdlib/builtin/lv_sprintf_builtin.c:42,776-794) → 플래그/정밀도만 소비되고 'f'가 literal로 출력됨
- [x] `sdkconfig.defaults`에 `CONFIG_LV_USE_FLOAT=y` 추가 + 기존 `sdkconfig`의 `# CONFIG_LV_USE_FLOAT is not set` 라인을 `CONFIG_LV_USE_FLOAT=y`로 교체 (sdkconfig 우선순위 때문)
- [ ] `idf.py build` (사용자 IDF 환경에서 수동) — idf.py가 호스트 PATH에 없어 클로드가 직접 못 돌림
- [x] flash & monitor로 Accel/Gyro가 `+0.12` 형식으로 정상 출력되는지 확인 (사용자: 자이로 정상)
- [x] AHT30 진단: 같은 모니터 로그에서 `sensors` TAG로 다음 패턴 확인 — `bsp_sensor_init(HUMITURE): ...`, `iot_sensor_start: ...`, `i2c_dock` 관련 에러. AHT30은 BOX-3 dock의 별도 I2C 버스(GPIO 40/41) 상에 있어 코드 자체는 맞음 (managed_components/espressif__esp-box-3/esp-box-3.c:884-893)
- [x] 사용자 로그 공유 (부트 ~1558ms까지)

## 2026-05-11 | 마이크/AHT30 추가 진단 + 마이크 게인 보정 (Espress_dev)

진단 정황: 부트 로그에서 IMU/AHT30 sensor_hub 생성+핸들러 등록 성공(`SENSOR_HUB`/`SENSOR_LOOP` 라인), ES7210 마이크 `Enable MIC1/MIC2`+`Unmuted`+`Adev_Codec: Open codec device OK`. PMOD1에는 공식 ESP32-S3-BOX-3-SENSOR 확장보드가 연결됨. 부트 로그가 t=1558ms에서 끊겨 첫 AHT30 polling 사이클 진입 전.

- [x] audio_check.c: `esp_codec_dev_set_in_gain(s_mic, 30.0f)` → `42.0f`로 올리고 `esp_codec_dev_open` **이전**으로 이동 (BSP API.md 예시와 동일 순서)
- [ ] `idf.py build && flash && monitor` (사용자 IDF 환경)
- [ ] **MUTE 슬라이드 스위치 확인** — BOX-3 본체 상단의 mic mute 슬라이드가 mute 위치면 ES7210 unmuted여도 logic gate가 GPIO_1을 통해 입력 차단. 우선 사용자 육안 확인 필요
- [ ] flash 이후 **최소 5초간** monitor 로그 캡처. `AHT30:` 태그의 `Failed to start AHT30 measurement` 또는 `timeout`이 찍히는지 확인 (sensor_hub polling 1초 주기 × 3-4회)
- [ ] 로그에 AHT30 에러 없는데 콜백 미진입이면 → `iot_sensor_handler_register_with_type` → `iot_sensor_handler_register` (handle 기반) 으로 전환, 공식 display_sensors 예제 패턴과 정렬

## 2026-05-11 | ToDo/LP repo-local 전환 + 환경 LP 추가 (Espress_dev)

- [x] CLAUDE.md §4 task management의 ToDo.md 경로를 `workspace root` → `project repo root (Espress_dev/ToDo.md)`로 변경
- [x] CLAUDE.md §9, §10의 LearnedPatterns.md 경로도 repo root로 동기화
- [x] LP §5.4 Zadig 드라이버 할당 (CDC + WinUSB v6) 항목 추가
- [x] LP §5.5 ESP-IDF VS Code 확장 + Ctrl+E 코드 단축키 항목 추가

## 2026-05-11 | AHT30 silent event drop 수정 (sensor_hub base 불일치)

가설/근거: `iot_sensor_handler_register_with_type(HUMITURE_ID, ...)`은 고정 매크로 base `SENSOR_HUMITURE_EVENTS`에 등록 (iot_sensor_hub.c:658), polling task는 동적 base `sensor->event_base` (sprintf "%s_%x" → "sensor_hub_aht30_38", iot_sensor_hub.c:251,359)로 post. esp_event base 불일치 → 콜백 0회 호출. 부트 로그는 `register succeed × 2`까지 정상 표시되어 silent drop. LV_USE_FLOAT과 같은 카테고리("라이브러리 silent data drop")지만 메커니즘은 API 변형 선택 문제.

- [x] `main/sensors.c:109-114`: `_with_type` × 2 호출과 `inst_t/inst_h` 로컬을 `iot_sensor_handler_register(s_humiture, humiture_event_cb, NULL)` 1줄로 교체
- [x] flash + monitor: 사용자 확인 — Temp/Hum 정상 갱신 ✅
- [x] LP §3.4에 `sensor_hub _with_type vs handle-based register 차이` 항목 영구 등록

## 2026-05-11 | 마이크 RMS 0 + beep 항상 실패 수정 (esp_codec_dev 반환 규약)

원인: `esp_codec_dev_read/write`는 POSIX read/write와 달리 **성공 시 0 (`ESP_CODEC_DEV_OK`)** 반환, 바이트 수 반환 아님 (audio_codec_data_i2s.c:717). 사용자 코드 `if (got > 0)` (audio_check.c:58)는 성공을 절대 인식 못 함 → mic_task가 RMS 업데이트 분기로 진입 못 함 → UI bar 0 고정. beep도 `if (written <= 0)`로 성공을 실패로 오인 (audio_check.c:94) → 상태 라벨 항상 "beep write fail". LV_USE_FLOAT, sensor_hub base 불일치에 이은 세 번째 silent data drop 패턴.

- [x] audio_check.c mic_task: `if (got > 0)` → `if (err == ESP_CODEC_DEV_OK)`로 교체, `frames = got/sizeof(int16_t)` → `MIC_CHUNK_FRAMES` 상수 사용
- [x] audio_check.c beep_task: `if (written <= 0)` → `if (err != ESP_CODEC_DEV_OK)`로 교체
- [x] LP §3.5에 `esp_codec_dev_read/write 반환 규약 (POSIX 아님)` 항목 등록
- [x] `idf.py build` 통과 확인 (사용자 IDF 환경 — 워닝 0)
- [x] flash + monitor: Audio 탭에서 마이크에 소리 입력 시 RMS bar가 즉시 반응 — 사용자 확인 ✅
- [x] Beep 버튼 / status 라벨 동작 확인 — 사용자 확인 ✅

## 2026-05-11 | Beszel(http://10.16.21.197:8090) 모니터링 탭 추가

계획 파일: `C:\Users\USER_55_DeepLearning\.claude\plans\beszel-http-10-16-21-197-8090-esp32-resilient-bee.md` (사용자 ExitPlanMode 승인 완료)

UX: 한 호스트씩 CPU/Memory/GPU 바 그래프. CONFIG(=이전) / MUTE(=다음) 버튼으로 호스트 전환. WPA2-Personal, 폴링 5초. GPU 없으면 회색 + "N/A". 자격증명은 menuconfig(`sdkconfig` 로컬, .gitignore 확인) 한정.

관련 LP: §5.2(idf.py PATH 없음 → 빌드/플래시는 사용자 환경), §2.1(sdkconfig 우선순위), §3.6(헤더로 시그니처 검증).

### 작업 항목

- [x] `main/Kconfig.projbuild` 신규: BESZEL_WIFI_SSID/PASSWORD/SERVER_URL/USER/PASSWORD/POLL_INTERVAL_S/MAX_HOSTS
- [x] `.gitignore`에 `sdkconfig` 추가 + `git rm --cached sdkconfig` (사용자 결정: 자격증명 누출 방지)
- [x] `main/network.h/c` 신규: WiFi STA 비차단 초기화 + auto-reconnect (`esp_wifi_connect` on disconnect) + `network_wait_connected`
- [x] `main/beszel.h/c` 신규: PocketBase 인증, /api/collections/systems/records 폴링, cJSON 파싱, 토큰 5h30m 사전 갱신 + 401 재인증 1회 재시도, `s_systems[]` 캐시, `beszel_select_prev/next`
- [x] `main/beszel.c` 첫 응답 1회 raw JSON 로깅(`s_logged_raw_systems` 플래그, 256바이트 청크). GPU 필드는 `g`/`gpu`/`gp` 순서로 root 또는 `info.{...}` 양쪽 탐색 — 첫 부팅 후 raw 로그에서 실제 경로 확정 시 `parse_one_system`의 `gpu_keys[]` 1줄 수정
- [x] `main/ui.h` 확장: `ui_beszel_host_t` + `ui_beszel_set_host/set_status/set_unavailable`. (계획에 있던 `ui_callbacks_t.on_btn_config/on_btn_mute`는 실제 호출 경로가 ui→cb가 아닌 buttons_check→cb라 추가하지 않음 — 죽은 필드 회피)
- [x] `main/ui.c`: 7번째 탭 "Beszel" (호스트명 + 상태닷 + N/M 인덱스 + CPU/MEM/GPU 바 + 푸터). `make_metric_row` 헬퍼로 3개 바 행 공통화. GPU 없을 때 회색 인디케이터 + "N/A"
- [x] `main/buttons_check.h/c`: `buttons_callbacks_t` 추가, `buttons_check_init(const buttons_callbacks_t *)`. on_short에서 카운터 업데이트 후 인덱스별 콜백 분기 — Btn 탭 카운터 표시 유지
- [x] `main/main.c`: `on_config_pressed`=`beszel_select_prev`, `on_mute_pressed`=`beszel_select_next` 콜백 + `buttons_check_init(&btn_cbs)` + `network_init()` + `beszel_init()` 호출
- [x] `main/CMakeLists.txt`: SRCS `network.c`/`beszel.c` 추가, REQUIRES `nvs_flash esp_wifi esp_netif esp_event esp_http_client esp-tls json`
- [x] `idf.py menuconfig`로 WiFi SSID/PW + BESZEL_USER/PW 입력 → `idf.py build && flash && monitor` 완료 (사용자 확인)
- [x] 시리얼 로그: `network: got IP 192.168.1.206`, `beszel: auth OK (token len=224)`, raw systems response 815 bytes 1회 출력 확인. `info.cpu/mp/dp/g` 필드 경로 확정 (idle 0%일 때 `info.g`가 omitempty로 빠지는 v0.18.x 동작 확인)
- [x] Beszel 웹 UI(H200Server/3090Server)와 CPU/MEM 수치 일치 — 사용자 확인 ✅
- [x] CONFIG/MUTE 버튼으로 탭 전환 동작 — 사용자 확인 ✅
- [x] **계획 중간 변경**: 단일 Beszel 탭 + 다른 모듈 탭 → "탭 = 호스트" 구성으로 전면 재설계. sensors.c/h, audio_check.c/h, ir_check.c/h 삭제하고 sensor_example/에 보존된 이전 스냅샷을 README에서 설명. ui.c는 동적 tabview rebuild 패턴으로 재작성
- [x] GPU 표시 결정: `info.g` 누락 시 N/A 대신 0% 표시 (omitempty 모호성 해소) — 사용자 결정
- [x] `LearnedPatterns.md` §3.7–3.10, §5.6에 신규 함정 5건 등록 (json 컴포넌트 v6.x 변경, NAME_MAX picolibc 충돌, uint32_t printf, info.g omitempty, gh CLI 부재시 portable 설치)
- [x] README.md 전면 재작성: Beszel monitor를 메인으로, sensor_example/를 이전 자체 진단 펌웨어 스냅샷으로 설명
- [x] GitHub Issue 생성: https://github.com/coport-uni/Esp32S3-CrawlerDisplay/issues/2
