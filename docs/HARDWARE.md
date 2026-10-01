# Elecrow CrowPanel ESP32-S3 4.2" E-Paper HMI (400x300 B/W): Hardware Reference for Firmware

*Author: Mark Castelluccio (markacastelluccio@gmail.com). Research gathered and written with assistance from Claude Code (Anthropic), 2026-09-30. Anything marked "inference" has not been checked on real hardware; section 10 records what has.*

**How to read this.** Each fact cites its source inline. The best source is **Elecrow's own Eagle schematic and board files** in their GitHub repo. I parsed the `.sch` and `.brd` XML directly and pulled out the netlist, so pin-to-net mappings below are exact. Other sources are ranked below that. Where sources disagree, the table says so.

Primary repo (master @ `cb6d6b4`, 2026-07-30):
https://github.com/Elecrow-RD/CrowPanel-ESP32-4.2-E-paper-HMI-Display-with-400-300

- Schematic: `Eagle_SCH&PCB/CrowPanel-ESP32-Display-4.2E-Inch/CrowPanel ESP32 Display-4.2(E) Inch.sch` (+ `.pdf`, `.brd`)
- PCB silkscreen (from `.brd`): **`SKU:DIE07300S`**, **`V1.0`**, `ESP32 Display-4.2(E) inch`
- Wiki: https://www.elecrow.com/wiki/CrowPanel_ESP32_E-paper_4.2-inch_HMI_Display.html (SKU given as **DIE07300S001**)

---

## 0. TL;DR GPIO map

| Function | GPIO | Polarity / notes | Confidence |
|---|---|---|---|
| EPD SCK | 12 | HW SPI default SCK on S3 Arduino | High (schematic + Elecrow code) |
| EPD MOSI (SDA) | 11 | HW SPI default MOSI on S3 Arduino. No MISO to the panel | High |
| EPD CS | 45 | **Strapping pin (VDD_SPI)**. No external pull-up | High |
| EPD DC | 46 | **Strapping pin (ROM log / boot mode)** | High |
| EPD RST | 47 | Direct, no RC "clever reset" circuit | High |
| EPD BUSY | 48 | Polarity **depends on panel controller** (see §2) | High |
| **EPD power enable** | **7** | **HIGH = panel on**. Drives a low-side 2N7002 (Q9) that switches the panel's GND | High |
| LED (red, D1, "POWER LED") | 41 | Active HIGH (4.7 kΩ series) | High |
| microSD power enable | 42 | HIGH = card on. Low-side 2N7002 (Q10) switches the card's GND | High |
| microSD CS | 10 | 10 kΩ pull-up | High |
| microSD MOSI | 40 | 10 kΩ pull-up | High |
| microSD MISO | 13 | 10 kΩ pull-up | High |
| microSD SCK | 39 | | High |
| MENU / "HOME" button (K3) | 2 | **Active LOW**, external 4.7 kΩ pull-up. Factory FW uses it as the ext0 deep-sleep wake source | High |
| EXIT button (K4) | 1 | **Active LOW**, external 4.7 kΩ pull-up | High |
| Rotary (dial) UP | 6 | **Active LOW**, external 4.7 kΩ pull-up | High |
| Rotary (dial) DOWN | 4 | **Active LOW**, external 4.7 kΩ pull-up | High |
| Rotary press (CONF/OK) | 5 | **Active LOW**, external 4.7 kΩ pull-up | High |
| BOOT button (K1) | 0 | Active LOW (module's internal pull-up) | High |
| RESET button (K2) | EN | 10 kΩ pull-up | High |
| UART0 TX / RX | 43 / 44 | Level-shifted to CH340C. No separate UART connector on the PCB | High |
| Free header GPIO | 3, 8, 9, 14, 15, 16, 17, 18, 19, 20, 21, 38 | 2x10 2.54 mm header with 4x 3V3 and 4x GND | High |
| Battery voltage ADC | **none** | BAT net reaches no GPIO | High (netlist) |
| RTC chip | **none** | Not in BOM | High (BOM) |
| Sensors | **none** | Not in BOM | High (BOM) |

---

## 1. MCU module, flash, PSRAM

| Item | Value | Source |
|---|---|---|
| Module | **ESP32-S3-WROOM-1-N8R8** | Schematic sheet title "ESP32-S3-WROOM-1-N8R8"; repo readme; wiki |
| Flash | 8 MB, quad SPI (QIO, 80 MHz) | Repo readme; CircuitPython `mpconfigboard.mk` (`CIRCUITPY_ESP_FLASH_MODE = qio`); matches your esptool output "8MB quad flash" |
| PSRAM | **8 MB OCTAL (OPI)**, embedded in the ESP32-S3R8 die | Elecrow Arduino tutorials say `PSRAM: "OPI PSRAM"` ([v1.0 tutorial](https://www.elecrow.com/wiki/CrowPanel_ESP32_E-Paper_4.2-inch_Arduino_Tutorial.html), [new tutorial](http://www.elecrow.com/wiki/New_E-Paper_4.2-inch_Arduino_Tutorial.html)); CircuitPython `CIRCUITPY_ESP_PSRAM_MODE = opi` ([board def](https://github.com/adafruit/circuitpython/tree/main/ports/espressif/boards/elecrow_crowpanel_4_2_epaper)); ESPHome `psram: mode: octal` ([UglyBob79/esphome-common crowpanel-4.2.yaml](https://github.com/UglyBob79/esphome-common/blob/master/hardware/esp32/crowpanel-4.2.yaml)) |
| Chip rev | ESP32-S3 (QFN56) rev v0.2 | Your esptool output, which matches the Elecrow forum report ([thread](https://forum.elecrow.com/discussion/comment/4333)) |

**Disagreement:** espboards.dev says Arduino `PSRAM: QSPI PSRAM` and gives an ESPHome example with `psram: mode: quad` ([espboards board page](https://www.espboards.dev/esp32/elecrow-crowpanel-epaper-4-2/)). **I think that is wrong.** The N8R8 module uses the ESP32-S3R8 chip, which has 8 MB *octal* PSRAM in the package. Esptool's "Embedded PSRAM 8MB (AP_3v3)" is exactly that part. Elecrow, CircuitPython and ESPHome all use OPI/octal.

**PlatformIO (known-good community config)**, from [Runpoint-Partners/crowpanel-usage-dashboard `firmware/platformio.ini`](https://github.com/Runpoint-Partners/crowpanel-usage-dashboard/blob/main/firmware/platformio.ini):
```ini
board = esp32-s3-devkitc-1
board_build.flash_size = 8MB
board_upload.flash_size = 8MB
board_build.partitions = huge_app.csv
board_build.flash_mode = qio
board_build.f_flash = 80000000L
board_build.arduino.memory_type = qio_opi
build_flags = -DBOARD_HAS_PSRAM
upload_speed = 115200
```
Consequence: GPIO35, 36 and 37 are used by octal PSRAM and are not available. They are also not routed on this board.

Elecrow's Arduino settings: Board "ESP32S3 Dev Module", Partition "Huge APP (3MB No OTA/1MB SPIFFS)", PSRAM "OPI PSRAM". **ESP32 Arduino core 2.0.14 is "explicitly required"** for their examples (new tutorial; also `example/readme.md` in the repo: "esp32 version needs to be 2.0.14").

---

## 2. E-paper panel and controller (READ THIS: two controllers ship under one SKU)

### Common facts
- 4.2", **400 (H) x 300 (V)**, black/white, active area 84.8 x 63.6 mm, pixel pitch 0.212 mm. Source: repo `readme.md`, wiki.
- 24-pin 0.5 mm FPC (U4). Schematic: `BS` tied to LCD_GND (4-wire SPI). `TSCL`/`TSDA` tied to LCD_GND. `VCI` and `VDDIO` go straight to the always-on 3V3 rail. The boost circuit (GDR/RESE, Si1308 Q4, 47 µH L2, MBR0530 diodes) is on the main PCB.
- **There is no MISO line to the panel**, so you cannot read panel registers. Write-only SPI.

### Two hardware versions

| | v1.0 ("old") | v1.2 / "green circular sticker on the back" ("new") |
|---|---|---|
| Controller | **SSD1683** | **UC8276C** (UC81xx command set) |
| Waveform | OTP on chip | Must be uploaded by the driver (LUT regs 0x20–0x24) |
| BUSY polarity | **HIGH = busy** | **LOW = busy** (BUSY_N) |
| GxEPD2 class | **`GxEPD2_420_GYE042A87`** (or `GxEPD2_420_GDEY042T81`) | **`GxEPD2_420_SE0420NQ04`** |
| Elecrow code folder | `example/arduino/` | `example/arduino_A_green_circular_sticker_on_the_back/` |
| Factory FW folder | `factory_firmware/Epaper-4.2(E) Inch-V1.0` | `factory_firmware/E-paper_4.2inch_PCBA-V1.2A_green_circular_sticker_on_the_back` (there is also `Epaper-4.2(E) Inch-V1.2`) |

Evidence:
- Elecrow repo `example/readme.md`: *"Version arduino_A_green_circular_sticker_on_the_back is a new release. In this version, Updated the display driver. All other features remain unchanged. ... has a green circular sticker on the back."* The repo readme and wiki still only name SSD1683.
- **Elecrow's new driver (`EPD.cpp` in the green-sticker folder) is a UC8276-style init.** It sends `0x00 PSR`, `0x01 PWR`, `0x06 BTST`, `0x30 PLL=0x09`, `0x61 TRES = 0x01 0x90 0x01 0x2C` (400x300), `0x60 TCON=0x22`, `0xE3=0x88`, LUT upload to `0x20..0x24`, and refreshes with `0x17 0xA5` (auto-sequence). It is almost byte-for-byte the init in GxEPD2's `GxEPD2_420_SE0420NQ04::_InitDisplay()` ("Controller: UC8276C").
- espboards blog ([crowpanel-4-2-epaper-v1-2-esphome-driver](https://www.espboards.dev/blog/crowpanel-4-2-epaper-v1-2-esphome-driver/)): v1.0 = SSD1683, v1.2 = UC8276C. The version is visible only as silkscreen next to the SKU on the PCB, so you have to open the case. **The green sticker was not reliable: their v1.2 unit had no sticker.** Using the wrong driver gives "a blank or garbled screen". The UC8276C framebuffer needs bit inversion compared with SSD1683.
- Elecrow forum ([comment 4333](https://forum.elecrow.com/discussion/comment/4333), [comment 4385](https://forum.elecrow.com/discussion/comment/4385)), on DIE07300S: *"BUSY (GPIO48) never clears after reset, no matter what firmware I compile myself – only the precompiled factory image works."* The working fix posted there: GxEPD2 1.6.9, **`GxEPD2_420_SE0420NQ04`**, ESP32 core 2.0.14, upload speed 115200, `display.init(115200, true, 2, false)`, and power pins 41 and 7 driven HIGH.
- Why that symptom fits (inference): an SSD1683 driver waits for BUSY to go LOW. On a UC8276C panel BUSY sits HIGH when idle, so the driver waits forever.

**Runtime detection idea (inference, untested):** apply power (GPIO7 HIGH), pulse RST, wait about 10–20 ms, then read GPIO48 while idle. **LOW ⇒ SSD1683 (v1.0)**, **HIGH ⇒ UC8276C (v1.2)**. Choose the driver class from that. With no MISO, BUSY polarity is the only signal the panel gives back.

**Your unit:** esptool cannot tell you the panel type. To find out, check the PCB silkscreen (V1.0 or V1.2) or the sticker, try both GxEPD2 classes, or use the BUSY test above.

### GxEPD2 classes and timings (GxEPD2 v1.6.9, https://github.com/ZinggJM/GxEPD2)

| Class | Controller | BUSY level in ctor | full_refresh_time | partial_refresh_time | Notes |
|---|---|---|---|---|---|
| `GxEPD2_420_GYE042A87` (`src/other/`) | SSD1683 (HINK-E042-A07-FPC-A1) | HIGH | 1200 ms | 400 ms | `hasPartialUpdate = hasFastPartialUpdate = true`. `useFastFullUpdate = true` (temperature-register trick, writes 0x1A=0x64). The header says to set it false for low temperatures, but it is a `static const` |
| `GxEPD2_420_GDEY042T81` (`src/gdey/`) | SSD1683 | HIGH | 1200 ms | 400 ms | Same code as GYE042A87 except it adds runtime `selectFastFullUpdate(bool)` and uses 0x6E (2024 panels) instead of 0x64. Also works on v1.0 (makerguides) |
| `GxEPD2_420_SE0420NQ04` (`src/other/`) | UC8276C (OPM042A2_V1.0) | LOW | 3200 ms | 620 ms | `usePartialUpdateWindow = false`: a partial update refreshes the whole screen with the differential waveform |

GxEPD2's own examples now carry CrowPanel hints (`examples/GxEPD2_Example/GxEPD2_Example.ino`):
```cpp
#if defined(ESP32) && defined(ARDUINO_ESP32S3_DEV) && true // CrowPanel
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH); // enable power to the panel
#endif
```
and in `GxEPD2_display_selection_new_style.h`:
```cpp
//GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ 45, /*DC=*/ 46, /*RST=*/ 47, /*BUSY=*/ 48)); // CrowPanel wiring
```

Other measured or claimed timings:
- makerguides (v1.0, GYE042A87): partial refresh "under 0.5 seconds", full refresh "several seconds" ([makerguides](https://www.makerguides.com/crowpanel-4-2-inch-e-paper-with-gxepd2/)).
- espboards ESPHome driver on v1.2 (UC8276C) with Elecrow's single-phase LUTs: GC (full) **1056 ms**, DU (partial) **448 ms**. Residue did not build up over 21 partial updates. They default to `full_update_every: 10` ([espboards blog](https://www.espboards.dev/blog/crowpanel-4-2-epaper-v1-2-esphome-driver/)).
- Elecrow v1.0 driver modes: `EPD_Update` (0x22=0xF7, full), `EPD_Update_Fast` (0x22=0xC7 after loading a fake temperature 0x1A=0x6E for "1.5 s" or 0x5A for "1 s"), `EPD_Update_Part` (0x22=0xFF).

---

## 3. GPIO and peripheral details (from the schematic netlist)

Net names come straight from the Eagle schematic.

### EPD
| Net | GPIO | FPC pin |
|---|---|---|
| `IO12_SPI_CLK` | 12 | SCL |
| `IO11_SPI_MOSI` | 11 | SDA |
| `IO45_CS` | 45 | CS |
| `IO46_D/C` | 46 | D/C |
| `IO47_RES` | 47 | RES |
| `IO48_BUSY` | 48 | BUSY |
| `IO7_LCD_3.3_CTL` | 7 | through R172 100 kΩ to the gate of **Q9 (2N7002)**. Q9 drain = `LCD_GND`, source = GND |

The PCB silkscreen (from `.brd`) reads: `IO48 => LCD_BUSY / IO47 => LCD_RES / IO46 => LCD_D/C / IO45 => LCD_SPI_CS / IO12 => LCD_SPI_CLK / IO11 => LCD_SPI_MOSI`.

**Power switch topology:** this is a **low-side (ground) switch**. The panel's VCI and VDDIO are always on 3V3. GPIO7 HIGH turns Q9 on and connects the panel ground. **There is no pull-down on Q9's gate.** The only path is the 100 kΩ series resistor to GPIO7, so if GPIO7 floats (for example in deep sleep without a hold), the gate state is undefined (inference from the schematic).

### microSD (TF)
`IO10_SPI_CS`, `IO40_SPI_MOSI`, `IO39_SPI_CLK`, `IO13_SPI_MISO` go to J1. There are 10 kΩ pull-ups to 3V3 on CS, MOSI, MISO, DAT1 and DAT2. `IO42_TF_3.3_CTL` drives the Q10 (2N7002) gate through 100 kΩ, and Q10 switches `TF_GND`. Card VDD is always 3V3. Silkscreen: `IO10 => SPI_CS / IO40 => SPI_MOSI / IO39 => SPI_CLK / IO13 => SPI_MISO`. The card is on a separate bus from the EPD. Elecrow uses `SPIClass(HSPI)` for it.

### Buttons (all active-low with external pull-ups to 3V3, no RC debounce)
| Ref | Net | GPIO | Pull-up |
|---|---|---|---|
| K3 "MENU" (Elecrow code: `HOME_KEY`) | `IO2_MENU` | 2 | R27 4.7 kΩ |
| K4 "EXIT" | `IO1_EXIT` | 1 | R28 4.7 kΩ |
| K5 rotary TM_2024A pin 2 | `IO6_UP` | 6 | R29 4.7 kΩ |
| K5 pin 1 | `IO4_DOWN` | 4 | R30 4.7 kΩ |
| K5 pin 4 (press) | `IO5_CONF` | 5 | R31 4.7 kΩ |
| K1 BOOT | `BOOT` | 0 | none external (also driven by the auto-reset transistor) |
| K2 RESET | `EN_RESET` | EN | R9 10 kΩ, C2 to GND |

All of GPIO1, 2, 4, 5 and 6 are RTC GPIOs, so they can wake from deep sleep (ext0/ext1). Elecrow's factory firmware uses `esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);` (MENU, active-low) (`factory_sourecode/4.2_tow/main/main.ino`).

Naming varies: Elecrow code calls GPIO2 `HOME_KEY`, GPIO6 `PRV_KEY`, GPIO4 `NEXT_KEY` and GPIO5 `OK_KEY`. CircuitPython calls them `BUTTON_MENU`, `ROCKER_UP`, `ROCKER_DOWN` and `ROCKER_CLICK`. The ESPHome config uses `INPUT_PULLUP` plus `inverted: true` for all five. Elecrow uses plain `INPUT`, which works because the pull-ups are external.

### LED
`IO41_LED` goes through R16 4.7 kΩ to D1 (red, "POWER LED"), cathode to GND. **Active HIGH.** R17 (marked NC) is an unpopulated option to tie the LED to 3V3. Elecrow comments call GPIO41 "power control" or "PWR". In the schematic it only drives the LED. Elecrow's 4.2_PWR example just toggles it ("PWR:ON/OFF"). CircuitPython: `MICROPY_HW_LED_STATUS = GPIO41`.

### 2x10 GPIO header (U5, 2.54 mm)
Symbol pin numbers, and physical layout from the `.brd` pad coordinates (top view, board-file orientation):

```
Row A (pins 1..10, left->right):  IO3  IO9  IO15 IO17 IO19 IO21 GND  GND  GND  GND
Row B (pins 20..11, left->right): IO8  IO14 IO16 IO18 IO20 IO38 3V3  3V3  3V3  3V3
```
So pin 1 = IO3, pin 20 = IO8, and pins 11–14 = 3V3, 7–10 = GND. The header's 3V3 is the always-on system rail and its GND is the hard ground (not switched).
- **GPIO19/GPIO20 are the ESP32-S3 native USB D-/D+.** They appear only on this header, not on the USB-C port.
- GPIO8/GPIO9 are the Arduino-ESP32 S3 default `SDA`/`SCL` ([pins_arduino.h](https://github.com/espressif/arduino-esp32/blob/master/variants/esp32s3/pins_arduino.h)). Use them for external I2C. **There is no on-board I2C device.**
- GPIO3 (JTAG source strap) is on the header, so do not pull it in a way that changes boot behaviour.
- Free ADC1-capable header pins: GPIO3, 8 and 9 (ADC1). Use one of these if you add a battery divider.

### UART0
`3V3_TX`/`3V3_RX` (GPIO43/44) pass through 2N7002 bidirectional level shifters (Q7/Q8, 5.6 kΩ pull-ups to 3V3 and to VBUS) to the CH340C, which runs from 5 V VBUS. **The schematic, BOM and `.brd` have no separate UART header.** The "UART0 x1" in Elecrow's spec list appears to mean this USB-serial path (wiki and readme list "UART0 x1" without describing a connector).

### Battery / charging
- B1 is a JST-SH 1.0 mm 2-pin connector (pin 1 = BAT+, pin 2 = GND). Wiki: "SH1.0, 2Pin", 3.7 V Li-ion/LiPo.
- U6 is a `4054A` (TP4054-class) linear charger fed from VBUS. PROG = R12 2 kΩ, which gives **about 500 mA charge current** (calculated as I = 1000 V / R_PROG). **The CHRG status pin is not connected**, so firmware cannot see charge status.
- Power path: VBUS goes through D2 (S2M) to VIN. BAT goes through D3 (SS12 Schottky) and Q3 (AO3401 P-MOSFET; gate pulled to VBUS by R10 1 kΩ and to GND by R11 10 kΩ) to VIN. So the battery feeds VIN through a low-loss PMOS when USB is absent.
- VIN goes to U2 **RY3420** synchronous buck (EN tied to VIN, always on) and then the 3V3 system rail. Feedback 45.3 kΩ/10 kΩ gives about 3.32 V.
- **Battery voltage is NOT readable.** The `BAT` net connects only to B1, D3, Q3, U6, C5/C6 and test pad P5. No divider, no GPIO. This contradicts mischianti.org ("Li-Po connector with charging & voltage monitoring", [mischianti](https://mischianti.org/crowpanel-esp32-s3-4-2-e-paper-hmi-display-high-resolution-pinout-datasheet-and-specs/)). espboards lists "Battery ADC: Not specified". To measure the battery, add a divider (for example 2 x 100 kΩ) from test pad P5 (BAT) to GPIO3, 8 or 9.

### RTC / sensors
The full BOM (from `.sch` and `.brd`) has no RTC chip, no 32 kHz crystal beyond the module's, no temperature or humidity sensor, no IMU and no buzzer. **No onboard RTC: confirmed.** Use the ESP32-S3 RTC timer plus NTP. The SSD1683 has an internal temperature sensor, but you cannot read it without MISO.

---

## 4. USB, serial, boot

- USB-C (J2) D+/D- go **only to the CH340C (U1)**. CC1/CC2 have 5.1 kΩ pull-downs (R1/R4), so it works as a USB-C sink. Your Mac shows it as `/dev/cu.wchusbserial*`.
- **Native USB-CDC/JTAG is NOT available on the USB-C port.** ESP32-S3 USB (GPIO19/20) goes only to the header. CircuitPython: *"This board doesn't have USB by default, it instead uses a CH340C USB-to-Serial chip"* (`CIRCUITPY_USB_DEVICE = 0`). For Arduino, use `USB CDC On Boot: Disabled` so that `Serial` = UART0.
- Auto-program: the classic DTR/RTS circuit with two S9013 NPNs (Q1/Q2) drives EN and IO0. Manual BOOT (IO0) and RESET (EN) buttons are also fitted.
- Upload speed: the community fixes (makerguides, Elecrow forum) use **115200**. Elecrow tutorials and espboards cite 921600. Inference: the 2N7002 level shifters with 5.6 kΩ pull-ups between the S3 and CH340 slow the edges, so high baud rates may be marginal. Fall back to 115200 if uploads fail.

---

## 5. Power, sleep, and switching the panel off

- Supply: USB-C 5 V or a 1-cell Li-ion on the SH1.0 connector. "Display Operation Voltage 2.2~3.7V" in Elecrow's spec refers to the panel, not the board input.
- **There are no published deep-sleep current figures** (wiki, product pages, makerguides, mischianti and hackaday all lack them). Measure it yourself.
- Likely contributors to sleep current (inference from the schematic, unverified):
  - ESP32-S3 deep sleep, roughly 8–10 µA (module).
  - RY3420 buck quiescent current.
  - **A possible leak through the UART level shifters when running on battery without USB.** With VBUS at about 0 V (pulled down via R10+R11), the 2N7002 body diodes of Q7/Q8 can conduct from the 3.3 V side (5.6 kΩ pull-ups R32/R33 hold it high) into R34/R35 and the unpowered CH340. That could be a few hundred µA. Measure before trusting any battery-life estimate.
  - Red LED if GPIO41 is left HIGH (about 0.3 mA). Drive it LOW or hold it.
- **Panel power between refreshes:** e-paper keeps the image with power removed. makerguides: power can be disabled after rendering; *"It will keep displaying it, even without any power."* Elecrow's factory firmware does **not** cut GPIO7 before deep sleep. It calls `EPD_RESET(); EPD_Sleep(); esp_deep_sleep_start();`. The controller's own deep sleep (SSD1683 `0x10 0x01`, or GxEPD2 `hibernate()`) is already in the µA range.
- If you do cut GPIO7 (inference):
  1. Call `display.hibernate()` first.
  2. Set CS/DC/RST/SCK/MOSI to high-Z or HIGH, not LOW. With the panel ground open, outputs driven LOW can back-feed the controller through its ESD diodes.
  3. In deep sleep, hold GPIO7 LOW explicitly (`gpio_hold_en` / `rtc_gpio_pulldown_en` plus `gpio_deep_sleep_hold_en`), because Q9's gate has no pull-down.
  4. Do the same with GPIO42 for the SD card.
- After any power cut or deep sleep, the controller RAM (the previous-frame buffer used for differential partial refresh) is lost. On wake:
  - Re-init with `display.init(..., initial=false ...)`.
  - Either do a full refresh first, or rewrite the previous image (GxEPD2 `writeImagePrevious` / `writeScreenBufferAgain` on SSD1683), before relying on fast partial updates. Otherwise expect ghosting or a wrong differential result.

---

## 6. Known gotchas (community and code review)

1. **GPIO7 must be HIGH before any SPI traffic.** Otherwise the panel is dead, which looks like a blank screen or BUSY never changing. (Every Elecrow example; makerguides; espboards; GxEPD2 example; CircuitPython `board_init()`.)
2. **Wrong controller driver means a frozen screen or BUSY never clears.** Use the GxEPD2 class that matches the panel revision (§2). This is the most common forum complaint.
3. **SPI pins:** EPD SCK 12 and MOSI 11 are the Arduino-ESP32-S3 *default* FSPI pins (`SCK=12, MOSI=11, MISO=13, SS=10`). That is why makerguides and mischianti get GxEPD2 working with no `SPI.begin` remap. The default `SPI.begin()` also claims GPIO13 (the SD card's MISO) as FSPI MISO. Being explicit is cleaner and is what the forum fix does:
   ```cpp
   SPI.begin(12 /*SCK*/, -1 /*MISO*/, 11 /*MOSI*/, 45 /*SS*/);
   display.init(115200, true, 10, false);   // or pass SPI + SPISettings in the extended init()
   ```
   Put the SD card on its own `SPIClass(HSPI)` with pins 39/13/40/10, as Elecrow does.
4. **Strapping pins:** EPD CS = GPIO45 (VDD_SPI strap; must not be pulled HIGH at reset) and DC = GPIO46 (boot-mode/ROM-log strap). Do not add external pull-ups to these.
5. **GxEPD2 constructor** (CS, DC, RST, BUSY):
   ```cpp
   // v1.0 (SSD1683)
   GxEPD2_BW<GxEPD2_420_GYE042A87, GxEPD2_420_GYE042A87::HEIGHT> display(GxEPD2_420_GYE042A87(45, 46, 47, 48));
   // v1.2 / green sticker (UC8276C)
   GxEPD2_BW<GxEPD2_420_SE0420NQ04, GxEPD2_420_SE0420NQ04::HEIGHT> display(GxEPD2_420_SE0420NQ04(45, 46, 47, 48));
   ```
   - The full 400x300 buffer is 15 000 bytes, which fits in RAM, so HEIGHT = full height (no paging).
   - `init()` arguments are (serial_baud, initial, reset_duration_ms, pulldown_rst_mode). Community values for reset_duration are 2 (forum), 50 (makerguides) and 10 (GxEPD2 default). There is no "clever reset" circuit, so anything from 2 to 20 ms works.
   - mischianti describes the 115200 argument as "SPI speed". It is actually the serial diagnostic baud rate.
6. **Partial refresh limits / ghosting:**
   - On UC8276C (GxEPD2 SE0420NQ04), partial = full-screen differential, about 450–620 ms.
   - espboards recommends a full refresh every about 10 partials (15–20 untested).
   - Elecrow's new-panel examples rewrote all demos to use full "fast" refresh, with the comment `// ===== Unified refresh (do NOT use partial refresh) =====` (`changed from EPD_Display_Part`). That is a hint that their partial refresh on the new panel was unsatisfactory.
   - Elecrow wiki: image width and height must be **multiples of 8**.
7. **Fast full refresh on SSD1683 uses a fake temperature** (0x1A register). It is fine at room temperature, but disable it (GDEY042T81 `selectFastFullUpdate(false)`) when cold.
8. **ESP32 core version:** Elecrow insists on arduino-esp32 **2.0.14**. The forum fix also used 2.0.14. A community PlatformIO build on pioarduino (core 3.x) works with Elecrow's bit-bang driver. GxEPD2 1.6.9 works on both.
9. **Elecrow new-driver quirks (code review, green-sticker `EPD.cpp`):**
   - `EPD_ReadBusy()` loops until BUSY==0. On UC8276C, LOW means busy, so the wait returns immediately. Elecrow papers over this with `delay(300)`/`delay(500)`.
   - `EPD_Display()` writes the image to command 0x24, which is a LUT register on UC8276C. Their examples use `EPD_Display_Fast()` (0x13) instead.
   - Do not copy these quirks.
10. **Boards reported dead or defective:** several users on the Elecrow forum (DIE07300S) report panels that never update even with the factory firmware. If neither driver class works and BUSY never moves, suspect hardware or the FPC seating.

---

## 7. Elecrow's own example code

- **Library:** Elecrow's own minimal **bit-banged (software) SPI** driver, copied into every sketch folder: `EPD.cpp/.h`, `EPD_SPI.cpp/.h`, `EPD_GUI.cpp/.h`, `EPD_font.h`.
  - `EPD_WR_Bus()` toggles SCK and MOSI with `digitalWrite`, and the `SPI.transfer` path is commented out.
  - **None of the examples use GxEPD2.** GxEPD2, EPaperDrive, a Waveshare-style `EPD` library and Adafruit_GFX are bundled in `example/arduino/libraries/` but unused by the 4.2 sketches. The readme's "dependency library" table lists `SSD1683 | EPD version=1.0.0`.
- The **pin header is identical** in both variants (byte-identical after stripping CR). Verbatim from `example/arduino/Examples/4.2_GPIO/EPD_SPI.h` and `example/arduino_A_green_circular_sticker_on_the_back/Examples/4.2_Example1_GPIO/EPD_SPI.h`:

```c
#ifndef _EPD_SPI_H_
#define _EPD_SPI_H_

#include <Arduino.h>

//项目板子
#define SCK 12
#define MOSI 11
#define RES 47
#define DC 46
#define CS 45
#define BUSY 48

//#define SCK 12
//#define MOSI 11
//#define RES 21
//#define DC 9
//#define CS 10
//#define BUSY 48

#define EPD_SCK_Clr() digitalWrite(SCK, LOW)
#define EPD_SCK_Set() digitalWrite(SCK, HIGH)

#define EPD_MOSI_Clr() digitalWrite(MOSI, LOW)
#define EPD_MOSI_Set() digitalWrite(MOSI, HIGH)

#define EPD_RES_Clr() digitalWrite(RES, LOW)
#define EPD_RES_Set() digitalWrite(RES, HIGH)

#define EPD_DC_Clr() digitalWrite(DC, LOW)
#define EPD_DC_Set() digitalWrite(DC, HIGH)

#define EPD_CS_Clr() digitalWrite(CS, LOW)
#define EPD_CS_Set() digitalWrite(CS, HIGH)

#define EPD_ReadBUSY digitalRead(BUSY)
...
#endif
```
(`//项目板子` means "project board". The commented-out block is a dev-board wiring and does not apply to the CrowPanel.)

`EPD.h`: `#define EPD_W 400`, `#define EPD_H 300`, `#define Fast_Seconds_1_5s 0`, `#define Fast_Seconds_1_s 1`.

- **Buttons**, verbatim from `4.2_Example2_KEY.ino` (green-sticker variant). Same values in v1.0 `4.2_key.ino` and in the factory `main.ino`:
```cpp
#define HOME_KEY 2   // Home key pin
#define EXIT_KEY 1   // Exit key pin
#define PRV_KEY 6    // Previous page key pin
#define NEXT_KEY 4   // Next page key pin
#define OK_KEY 5     // Confirm key pin
...
  pinMode(41, OUTPUT);    // Set GPIO41 as output (power control)
  digitalWrite(41, HIGH); // Enable power
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH); // Turn on the screen power
  pinMode(HOME_KEY, INPUT);
  ...
  if (digitalRead(HOME_KEY) == 0) {   // pressed = LOW
```
- **SD card**, verbatim from `4.2_Example4_TFCard.ino`:
```cpp
#define SD_MOSI 40
#define SD_MISO 13
#define SD_SCK 39
#define SD_CS 10

SPIClass SD_SPI = SPIClass(HSPI);
...
  // Turn on SD card power.
  pinMode(42, OUTPUT);
  digitalWrite(42, HIGH);
  delay(10);
  SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS, SD_SPI, 80000000)) {
```
- **Free GPIO list**, verbatim from `4.2_Example1_GPIO.ino`: `int pin_Num[12] = {8, 3, 14, 9, 16, 15, 18, 17, 20, 19, 38, 21};`
- **Deep sleep**, from the factory `main.ino`: `esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);` then, after a 60 s idle counter, `EPD_RESET(); EPD_Sleep(); esp_deep_sleep_start();`.

---

## 8. Source disagreements (summary)

| Topic | Says | Source | Verdict |
|---|---|---|---|
| PSRAM mode | QSPI / `mode: quad` | espboards board page | **Wrong.** Octal per Elecrow tutorial (OPI), CircuitPython (opi), ESPHome (octal), and the N8R8 + esptool "embedded 8MB" |
| Panel controller | SSD1683 only | Elecrow readme and wiki, mischianti, makerguides | Only true for v1.0. v1.2 / green sticker = UC8276C (Elecrow new driver, GxEPD2 SE0420NQ04, espboards, forum) |
| Battery voltage monitoring | "charging & voltage monitoring" | mischianti | **Wrong per schematic.** Charging only, no ADC tap, CHRG pin unconnected |
| GPIO41 | "power control" / "PWR" | Elecrow code comments | Schematic: only drives the red LED (`IO41_LED`) |
| UART0 interface | "UART0 x1" connector | Elecrow spec, retailers | No separate UART connector in `.sch` or `.brd`. UART0 = CH340C over USB-C |
| GxEPD2 support | "no direct support" (Dec 2024) | makerguides, hackaday | Out of date. GxEPD2 1.6.9 has GYE042A87 and SE0420NQ04 and CrowPanel wiring hints in its examples |
| Green sticker identifies v1.2 | Yes | Elecrow `example/readme.md` | espboards' v1.2 unit had no sticker. Check the PCB silkscreen V1.0/V1.2 instead |
| Upload speed | 921600 | espboards | Community fixes use 115200 |

## 9. All sources

- Elecrow GitHub repo (schematic, board, examples, factory source): https://github.com/Elecrow-RD/CrowPanel-ESP32-4.2-E-paper-HMI-Display-with-400-300
- Elecrow wiki (product): https://www.elecrow.com/wiki/CrowPanel_ESP32_E-paper_4.2-inch_HMI_Display.html
- Elecrow Arduino tutorial (v1.0): https://www.elecrow.com/wiki/CrowPanel_ESP32_E-Paper_4.2-inch_Arduino_Tutorial.html
- Elecrow Arduino tutorial (new / green sticker): http://www.elecrow.com/wiki/New_E-Paper_4.2-inch_Arduino_Tutorial.html
- Elecrow forum (DIE07300S frozen panel, SE0420NQ04 fix): https://forum.elecrow.com/discussion/comment/4333 and https://forum.elecrow.com/discussion/comment/4385
- GxEPD2 v1.6.9: https://github.com/ZinggJM/GxEPD2 (`src/other/GxEPD2_420_GYE042A87.*`, `src/other/GxEPD2_420_SE0420NQ04.*`, `src/gdey/GxEPD2_420_GDEY042T81.*`, `examples/GxEPD2_Example/`)
- makerguides GxEPD2 tutorial: https://www.makerguides.com/crowpanel-4-2-inch-e-paper-with-gxepd2/
- mischianti pinout: https://mischianti.org/crowpanel-esp32-s3-4-2-e-paper-hmi-display-high-resolution-pinout-datasheet-and-specs/
- mischianti GxEPD2/SSD1683: https://mischianti.org/ssd1683-eink-display-with-gxepd-and-esp32-and-crowpanel-4-2-hmi-basics-and-configuration/
- espboards board page: https://www.espboards.dev/esp32/elecrow-crowpanel-epaper-4-2/
- espboards v1.2 ESPHome driver blog: https://www.espboards.dev/blog/crowpanel-4-2-epaper-v1-2-esphome-driver/
- CircuitPython board definition: https://github.com/adafruit/circuitpython/tree/main/ports/espressif/boards/elecrow_crowpanel_4_2_epaper
- ESPHome hardware package: https://github.com/UglyBob79/esphome-common/blob/master/hardware/esp32/crowpanel-4.2.yaml
- PlatformIO community config: https://github.com/Runpoint-Partners/crowpanel-usage-dashboard/blob/main/firmware/platformio.ini
- Arduino-ESP32 S3 default pins: https://github.com/espressif/arduino-esp32/blob/master/variants/esp32s3/pins_arduino.h
- hackaday project (no power data): https://hackaday.io/project/202534-crowpanel-esp32-42-e-paper-wi-fi-info-display/details

---

## 10. Bring-up record: 2026-09-30

One unit, connected over its own USB-C to a Mac (`/dev/cu.wchusbserial2120`,
no extra driver needed), esptool v5.3.0.

| Check | Result |
|---|---|
| Chip | ESP32-S3 (QFN56) rev v0.2, "Embedded PSRAM 8MB (AP_3v3)", MAC `a4:cb:8f:e2:98:74` |
| Flash | 8 MB, manufacturer 0x20 device 0x4017, quad mode set in eFuse |
| Factory partition table | `nvs 20K, otadata 8K, app0 3264K, app1 3264K, spiffs 1536K, coredump 64K`, the core's `default_8MB.csv` |
| Factory firmware | Built with arduino-esp32 2.0.14 (paths in the image), BLE included, prints `Going to sleep now ....` and deep-sleeps. Bootloader through app0 (0x0-0x340000) backed up, SHA-256 `2bf797db874844e0420ec0f209aeefda7e2f92c8dd3430780b09ab78ae3dcd5b` |
| Serial speed | `read-flash` failed with "Serial data stream stopped" at 921600 and 460800; succeeded at 230400. Uploads at 230400 verify every time |
| Panel controller | BUSY high in 20 of 20 samples over 100 ms after reset, so **UC8276C (V1.2)**. The case was not opened to read the silkscreen |
| Refresh timing, `GxEPD2_420_SE0420NQ04` | power on 62 ms, full refresh 3.09 s, power off 40 ms |
| Firmware on core 3.3.9 (pioarduino 55.03.39) with `qio_opi` | Boots, OTA kit reports `app0, serial-flashed`, the panel draws |
| Partial refresh, same driver | 0.60 s busy, about 0.83 s including power on and off. On the UC8276C a "partial" refresh is a full-screen differential update |
| Network | Wi-Fi, SNTP, MQTT and Open-Meteo all working; OTA push to app1 confirmed after 30 s |
| Gotcha | GxEPD2 writes CS (45) and DC (46) before `pinMode()`, which core 3.x logs as `IO 45 is not set as GPIO`. Harmless; the firmware sets both as outputs first |

Still unverified: partial-refresh residue over long runs on this UC8276C, the
buttons on hardware, the microSD card, deep-sleep current, and the battery
path.

