# Temperature Clock — Elecrow CrowPanel ESP32 E-Paper HMI 4.2"

Firmware for the Elecrow CrowPanel ESP32 E-Paper HMI 4.2-inch display (SKU
`DIE07300S`, ESP32-S3-WROOM-1-N8R8, 400x300 black/white e-paper). It is the
e-paper sibling of the
[CrowPanel Advance 7.0 temperature display](https://github.com/robominds/Crowpanel-Advance-7.0-HMI-Display)
and shows the same readings:

- **The time and date**, from the network (SNTP). This board has no real-time
  clock, so after a power cut the clock reads `--:--` until Wi-Fi is up.
- **Maple Valley outdoors**, from the [Open-Meteo](https://open-meteo.com/)
  public API over plain HTTP. No API key.
- **Indoors**, from one of two places, decided at boot:
  - **An AHT10 on the header** (SDA GPIO8, SCL GPIO9). The panel shows it and
    publishes every ten-second sample to the broker. This unit is configured
    as **JennOffice**, publishing `JennOffice/AHT10/tempc` and
    `JennOffice/AHT10/hum` as bare decimals.
  - **No sensor answers**: the panel subscribes to the same topics instead, for
    example `MarkOffice/DHT/tempc` and `/hum` to show Mark's office.

  The room's label, its topics and the panel's network name come from
  `secrets.ini`. The serial log says which role the panel took:
  `aht10: found at 0x38 (status 0x18), publishing JennOffice/AHT10/tempc`, or
  `aht10: no sensor at 0x38, reading the broker instead`.

## The two views

**Clock**, shown at power-up: the time in large digits, the date beneath, and
the outdoor reading on the left and the indoor one on the right, each with its
humidity and a caption.

**Charts**: the same two readings, each with twelve hours of history. The
history lives in RAM and starts empty after every boot or update.

A reading whose source has gone quiet keeps its last number but is struck
through and marked `stale`: after a minute without an indoor reading,
after 45 minutes for the weather. A reading that has never arrived shows `--`.
A status line along the bottom appears only when Wi-Fi or the broker is down.

## Buttons

| Button | Action |
| --- | --- |
| MENU (HOME), or the dial up/down | Switch between the clock and the charts |
| EXIT | Toggle Fahrenheit and Celsius |
| Dial press | Full refresh now, to clear any ghosting |

The view and the units are saved in flash, so the panel comes back the way it
was left. That matters on e-paper: the last picture stays on the glass with the
power off, and a panel that came back different would look broken.

## How it refreshes

An e-paper refresh is slow and visible, so the firmware never redraws "every
loop". Each pass builds a text description of what the panel should say
(`lib/frame`) and refreshes only when it differs from what was last drawn:

- **The clock ticking over a minute**, or a change a reader would act on (a
  source going stale, the first reading arriving, units, view): a partial
  refresh right away, about 0.6 s on this panel.
- **Only numbers moving** (the indoor reading arrives every ten seconds): at most one
  partial refresh a minute, which in practice rides along with the clock.
- **Every 30th refresh, a view change, or the dial press**: a full refresh,
  about 3.1 s with the black-white flash, which clears partial-refresh
  residue. With the clock ticking, that is once every half hour.

## Two panels ship under this SKU

Elecrow sell this board with one of two e-paper controllers, and nothing
outside the case says which:

| Board | Controller | BUSY while busy | GxEPD2 class |
| --- | --- | --- | --- |
| V1.0 | SSD1683 | HIGH | `GxEPD2_420_GDEY042T81` |
| V1.2 ("green sticker", not always present) | UC8276C | LOW | `GxEPD2_420_SE0420NQ04` |

Using the wrong driver gives a panel that never updates, and the Elecrow forum
is full of exactly that complaint. This firmware compiles both and picks one
at boot from the BUSY level after a reset: the SSD1683 idles LOW and the
UC8276C idles HIGH. The serial log says which it chose:

```
epd: BUSY high in 20 of 20 samples after reset
epd: driving it as UC8276C (board V1.2)
```

To skip the probe, add `-DEPD_PANEL_SSD1683` or `-DEPD_PANEL_UC8276` to
`build_flags`. Full detail in `docs/HARDWARE.md` section 2.

## Build and flash

```sh
cp secrets.ini.example secrets.ini     # then fill in Wi-Fi and OTA password
pio run -e epaper42 -t upload          # build and flash over USB
pio run -e epaper42_ota -t upload      # afterwards: update over WiFi
pio test -e native                     # 108 host tests, no hardware needed
```

`secrets.ini` holds the Wi-Fi credentials, the MQTT broker and the indoor
topics, both captions, the weather coordinates, and the over-the-air hostname
(`JennOffice-Display` on this unit) and password. It is gitignored. Put every value
except `mqtt_port` in double quotes, and keep `"`, `'`, `\`, `$`, `` ` `` and
`;` out of the values. Every environment reads it, the host tests included;
for those, the unedited example is enough.

The build fetches the private library `robominds/esp32-ota-kit` (tag `v1.0.0`)
over SSH, so it needs read access to that repository.

Uploads run at 230400 baud. On this unit, reading the flash back failed at both
921600 and 460800. The serial lines pass through 2N7002 level shifters on the
way to the CH340C, which slow the signal edges. The CH340C on this board
shows up on macOS as `/dev/cu.wchusbserial*` with no extra driver.

### Updating over WiFi

The partition table is `default_8MB.csv`, the same as the factory firmware's:
two 3.2 MB app slots. After the first USB flash,
`pio run -e epaper42_ota -t upload` sends the build to
`<device_host>.local`, here `JennOffice-Display.local`. Changing `device_host`
needs one push to the OLD name, because the running firmware answers only to
the name it was built with:
`pio run -e epaper42_ota -t upload --upload-port <old-name>.local`.
The panel shows "Updating firmware" once at the start and "Restarting" at the
end. It draws no progress bar, because every partial refresh would stall the
transfer. A new image is confirmed 30 s after Wi-Fi comes up. One that crashes
before then is rolled back by the bootloader. Power-cycling within about 40 s
of an update also rolls back a good image; run the update again.

`firmware.bin` contains the Wi-Fi and OTA passwords as plain strings, and espota
traffic is authenticated but not encrypted. Use it on a network you trust.

## Layout

| Path | Contents |
| --- | --- |
| `lib/frame/` | The text each view prints, and the refresh policy. Pure C++, host-tested. |
| `lib/button/` | Debouncing that reports one event per press. Pure C++, host-tested. |
| `lib/history/`, `lib/channel/`, `lib/parse/` | Sample history, per-channel staleness, and MQTT/Open-Meteo parsing, shared with the 7-inch project. |
| `lib/aht10/` | The AHT10's six-byte frame, converted and sanity-checked. Shared with the 7-inch project. |
| `test/` | 108 host tests across the six libraries. |
| `src/board_pins.h` | Every GPIO, named, with its schematic net. |
| `src/ui.*` | Panel detection, the two views and the update messages, through GxEPD2 and U8g2 fonts. |
| `src/net.*` | Wi-Fi with non-blocking reconnection. |
| `src/clock.*` | Timezone and SNTP; there is no RTC chip. |
| `src/source_aht10.*` | The local AHT10: probe at boot, then a non-blocking ten-second sample. |
| `src/source_mqtt.*` | The indoor reading: published when the AHT10 is fitted, subscribed otherwise. |
| `src/source_weather.*` | The Maple Valley reading, polled from Open-Meteo every 15 minutes. |
| `src/settings.*` | View and units, persisted in NVS. |
| `src/update_screen.*` | What the panel shows during an over-the-air update. |
| `src/main.cpp` | Startup order, buttons, and the refresh loop. |
| `docs/HARDWARE.md` | The source-cited hardware reference this README summarizes. |

## Hardware at a glance

ESP32-S3-WROOM-1-N8R8: 8 MB quad flash, 8 MB **octal** PSRAM
(`memory_type = qio_opi`). Serial and upload go through a CH340C. The USB-C
port has no native USB. Panel on SPI: SCK 12, MOSI 11, CS 45, DC 46, RST 47,
BUSY 48. The panel's power switch is GPIO7, which must be HIGH before any SPI
traffic. Buttons, all active low: MENU 2, EXIT 1, dial up 6, down 4, press 5.
Red LED on GPIO41. microSD on its own bus (CS 10, MOSI 40, MISO 13, SCK 39,
power GPIO42). There is a LiPo charger but no battery-voltage ADC, and no RTC
and no sensors. Twelve spare GPIOs are on the 2x10 header. Everything is in
`docs/HARDWARE.md`.

## Verified so far

- `pio test -e native`: 108 host tests pass.
- `pio run -e epaper42` builds: flash 1,335,003 of 3,342,336 bytes (39.9%, one
  OTA slot); internal RAM 60,432 of 327,680 bytes (18.4%).
- Hardware bring-up, 2026-09-30, over USB at 230400:
  - esptool confirmed an ESP32-S3 rev v0.2 with embedded 8 MB PSRAM and 8 MB
    flash. The factory partition table is `default_8MB.csv`.
  - The panel probe chose UC8276C (BUSY high in 20 of 20 samples). The first
    full refresh measured 3.09 s busy, a real refresh time and not the instant
    return of a mismatched driver.
  - The factory image (bootloader through app0) is backed up locally under
    `backup/`, which is gitignored.
  - On the network: Wi-Fi associated, SNTP set the clock within seconds, MQTT
    subscribed to the office topics, and Open-Meteo returned Maple Valley.
    Partial refreshes measured 0.60 s busy (about 0.83 s including power
    on/off), full refreshes 3.09 s.
  - Both views and the buttons, checked on the panel, work as designed.
  - Over-the-air push to `epaper-display.local`: the image went to app1 and
    was confirmed after the 30 s window, and it reported `confirmed` on the
    next reset. The first push attempt failed at 0% with "Connection reset by
    peer"; the retry a minute later succeeded with the same build, and the
    cause is not yet known.
  - AHT10 on the header, 2026-09-30 evening: found at 0x38 with status
    `0x18` (already calibrated), the same reading as the bedroom panel's
    module, which is an AHT20-family die sold as an AHT10. The panel
    publishes `JennOffice/AHT10/tempc` and `/hum` every ten seconds, and a
    `mosquitto_sub` on the broker received them (23.5 C, 48.1 %). The renamed
    firmware answers to `JennOffice-Display.local` and was confirmed after an
    over-the-air push.

## License

MIT. See [LICENSE](LICENSE).

## Authorship

Mark Castelluccio <markacastelluccio@gmail.com>

Developed with Claude Code (Anthropic Claude Opus 5.5).
