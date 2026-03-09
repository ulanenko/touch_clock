# Touch Clock

Clock app for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-4C** (4-inch, 720x720 round IPS touch display).

Built with ESP-IDF v5.5+ and LVGL 9.3.

## Hardware

| Component | Spec |
|-----------|------|
| MCU | ESP32-P4 (dual-core RISC-V, 400MHz) |
| Display | 4-inch 720x720 IPS, MIPI-DSI (JD9365 driver) |
| Touch | GT911 capacitive, I2C (GPIO7/GPIO8) |
| Memory | 32MB PSRAM, 32MB NOR Flash |
| Module | [Waveshare ESP32-P4-WIFI6-Touch-LCD-4C](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C) |

## Features

- **Digital clock face** — oversized HH:MM with a larger date readout
- **Analog face** — classic dial with hour, minute, and second hands
- **Matrix face** — full-screen green dot-matrix time display
- **Wharton face** — amber LED ring with dot-matrix HH:MM center
- **Slava face** — ported light analog Slava dial, scaled from the original art for the 720x720 round panel
- **Slava Dark face** — dark analog Slava dial matched to the same round-display crop
- **Swipe left/right** to switch between six faces
- **Swipe up** to open a brightness menu with a slider for display backlight control
- **Touch settings UI** for Wi-Fi onboarding, timezone selection, alarms, snooze, and night mode
- **Wi-Fi station mode + SNTP** time sync through the onboard ESP32-C6 companion radio
- **Five configurable alarms** with weekday masks and snooze
- **Sunrise ramp** that increases brightness during the 30 minutes before the next alarm
- **Night mode** with its own clock face and dimmed brightness window
- Page indicator dots

## Project structure

```
touch_clock/
├── CMakeLists.txt          # ESP-IDF project root
├── partitions.csv          # 8MB app + 7MB storage
├── sdkconfig.defaults      # Target: esp32p4, 4-inch 720x720
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml   # LVGL, Waveshare BSP, hosted Wi-Fi
│   ├── app_settings.c      # Persistent settings + timezone handling
│   ├── alarm_logic.c       # Alarm, snooze, sunrise, night mode runtime
│   ├── wifi_time.c         # Wi-Fi station + SNTP integration
│   ├── clock_ui.c          # Faces and touch settings UI
│   └── main.c              # App bootstrap and glue
├── demos/                  # Waveshare sample projects (reference)
│   ├── Arduino/
│   └── ESP-IDF/
└── README.md
```

## Prerequisites

- **ESP-IDF v5.5+** — [install guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/)
- USB cable (Type-A to Type-C) connected to the **Type-C USB-to-UART** port on the board

## Quick start

```bash
# 1. Source ESP-IDF environment
source ~/esp/v5.5.3/esp-idf/export.sh   # adjust path to your install

# 2. Set target (first time only — creates sdkconfig and fetches components)
idf.py set-target esp32p4

# 3. Build
idf.py build

# 4. Flash
idf.py -p /dev/cu.usbmodem2101 flash

# 5. Monitor serial output
idf.py -p /dev/cu.usbmodem2101 monitor
# Quit monitor: Ctrl+]
```

Replace `/dev/cu.usbmodem2101` with your actual serial port. On Linux it's typically `/dev/ttyACM0`.

## Common commands

| Command | What it does |
|---------|-------------|
| `idf.py build` | Compile the project |
| `idf.py flash` | Flash firmware to device |
| `idf.py monitor` | Open serial monitor (115200 baud) |
| `idf.py flash monitor` | Flash then immediately monitor |
| `idf.py menuconfig` | Open SDK configuration editor |
| `idf.py fullclean` | Delete all build artifacts and start fresh |
| `idf.py set-target esp32p4` | (Re)configure for ESP32-P4 target |
| `idf.py size` | Show firmware size breakdown |

## Diagnostics

**Device not found / serial port errors:**

```bash
# List USB serial devices
ls /dev/cu.usb*          # macOS
ls /dev/ttyACM* /dev/ttyUSB*  # Linux

# Check if another process holds the port
lsof /dev/cu.usbmodem2101
```

**Build fails with component errors:**
```bash
# Wipe managed components and rebuild
rm -rf managed_components build
idf.py set-target esp32p4
idf.py build
```

**Chip revision mismatch:**
The `sdkconfig.defaults` sets `CONFIG_ESP32P4_REV_MIN_FULL=100` to support chip revision v1.x. If you see revision errors, verify your board's chip revision in the boot log and adjust if needed.

**Display doesn't turn on:**
Check that `CONFIG_BSP_LCD_TYPE_720_720_4_INCH=y` is set in `sdkconfig.defaults`. For the 3.4-inch 800x800 variant, change this to `CONFIG_BSP_LCD_TYPE_800_800_3_4_INCH=y` and update `SCREEN_SIZE` / radii in `main.c`.

**LVGL lock errors on boot:**
`E esp_lvgl:adapter: Failed to acquire LVGL lock` — harmless race during startup. The lock timeout is set to 1000ms which handles this in normal operation.

## Time

The clock boots from the last successfully synced timestamp when one is stored. Once Wi-Fi connects, SNTP refreshes the system clock; until then, the app falls back to a fixed seed epoch so the UI never starts at Unix epoch zero.

## Key dependencies

| Component | Version | Source |
|-----------|---------|--------|
| LVGL | ~9.3 | [lvgl/lvgl](https://github.com/lvgl/lvgl) via IDF Component Manager |
| Waveshare BSP | 2.0.0 | [waveshare/esp32_p4_wifi6_touch_lcd_xc](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_xc) |
| ESP LVGL Adapter | 0.1.4 | Transitive via BSP |
