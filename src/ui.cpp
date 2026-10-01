// Implementation of the e-paper views.
//
// 400 x 300, landscape, one bit per pixel. GxEPD2 keeps the whole frame in a
// 15 KB buffer, so every view is drawn in a single page.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include "ui.h"

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>

#include <cmath>
#include <new>

#include "board_pins.h"

namespace ui {
namespace {

// Two different controllers ship under this one SKU, with opposite BUSY
// polarity, and nothing printed outside the case says which (docs/HARDWARE.md
// section 2). Both drivers are compiled in and the panel picks one at boot.
// ENABLE_GxEPD2_GFX gives them a common virtual base, so everything below
// draws through one reference whichever was chosen.
using Ssd1683 = GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT>;
using Uc8276  = GxEPD2_BW<GxEPD2_420_SE0420NQ04, GxEPD2_420_SE0420NQ04::HEIGHT>;

GxEPD2_GFX* g_display = nullptr;

U8G2_FOR_ADAFRUIT_GFX g_text;

// After a hardware reset the SSD1683 (board V1.0) holds BUSY high only while
// it reloads, then idles LOW. The UC8276C (V1.2, "green sticker") drives
// BUSY_N, which idles HIGH. So a BUSY level that stays high for well past any
// reload time means the UC8276C. Sampled over 100 ms rather than once, so a
// slow reload is not mistaken for the other panel.
Panel probePanel() {
#if defined(EPD_PANEL_SSD1683)
    return Panel::Ssd1683;
#elif defined(EPD_PANEL_UC8276)
    return Panel::Uc8276;
#else
    pinMode(board::EPD_BUSY, INPUT);
    pinMode(board::EPD_RST, OUTPUT);
    digitalWrite(board::EPD_RST, LOW);
    delay(10);
    digitalWrite(board::EPD_RST, HIGH);

    uint8_t high = 0;
    for (int i = 0; i < 20; ++i) {
        delay(5);
        if (digitalRead(board::EPD_BUSY) == HIGH) ++high;
    }
    // The last half of the window decides; the first half is reload time.
    Serial.printf("epd: BUSY high in %u of 20 samples after reset\n", high);
    return high >= 15 ? Panel::Uc8276 : Panel::Ssd1683;
#endif
}

Panel g_panel = Panel::Ssd1683;

constexpr int16_t W = board::EPD_WIDTH;
constexpr int16_t H = board::EPD_HEIGHT;

// UTF-8 degree sign; U8g2's _tf fonts carry it.
constexpr char DEG[] = "\xC2\xB0";

// ---------------------------------------------------------------------------
// Text helpers. U8g2 positions text by its baseline.
// ---------------------------------------------------------------------------

int16_t textWidth(const uint8_t* font, const char* s) {
    g_text.setFont(font);
    return g_text.getUTF8Width(s);
}

void textAt(const uint8_t* font, int16_t x, int16_t baseline, const char* s) {
    g_text.setFont(font);
    g_text.drawUTF8(x, baseline, s);
}

void textCentred(const uint8_t* font, int16_t cx, int16_t baseline, const char* s) {
    textAt(font, cx - textWidth(font, s) / 2, baseline, s);
}

void textRight(const uint8_t* font, int16_t right, int16_t baseline, const char* s) {
    textAt(font, right - textWidth(font, s), baseline, s);
}

// A stale reading keeps its number - it is still the last thing known - but is
// struck through and captioned, the e-paper equivalent of the 7-inch panel's
// dimming.
void strike(int16_t x, int16_t y, int16_t w) {
    g_display->drawFastHLine(x, y, w, GxEPD_BLACK);
    g_display->drawFastHLine(x, y + 1, w, GxEPD_BLACK);
}

const char* unitSuffix(bool fahrenheit) { return fahrenheit ? "F" : "C"; }

// ---------------------------------------------------------------------------
// Clock view
// ---------------------------------------------------------------------------
//
//   +---------------------------------------------+
//   |               5:11  PM                      |   time, 92 px digits
//   |        Wednesday 30 September 2026          |
//   +----------------------+----------------------+
//   |       57.2°F         |       70.9°F         |   outdoor | indoor
//   |       81% RH         |       44% RH         |
//   |    Maple Valley      |       Office         |
//   +----------------------+----------------------+
//   | No broker                                   |   only when something is down

void drawReading(const frame::Frame& f, size_t i, int16_t cx, const char* label) {
    char big[16];
    snprintf(big, sizeof big, "%s", f.temp[i]);

    const uint8_t* num_font  = u8g2_font_logisoso42_tn;
    const uint8_t* unit_font = u8g2_font_helvB18_tf;

    char unit[8];
    snprintf(unit, sizeof unit, "%s%s", DEG, unitSuffix(f.fahrenheit));

    const int16_t num_w  = textWidth(num_font, big);
    const int16_t unit_w = f.valid[i] ? textWidth(unit_font, unit) : 0;
    const int16_t x0     = cx - (num_w + 4 + unit_w) / 2;
    constexpr int16_t BASE = 220;

    textAt(num_font, x0, BASE, big);
    if (f.valid[i]) textAt(unit_font, x0 + num_w + 4, BASE - 22, unit);
    if (f.valid[i] && f.stale[i]) strike(x0 - 4, BASE - 21, num_w + 8);

    char small[32];
    if (!f.valid[i]) {
        snprintf(small, sizeof small, "no reading yet");
    } else if (f.stale[i]) {
        snprintf(small, sizeof small, "%s  stale", f.hum[i]);
    } else {
        snprintf(small, sizeof small, "%s", f.hum[i]);
    }
    textCentred(u8g2_font_helvR14_tf, cx, 248, small);
    textCentred(u8g2_font_helvB14_tf, cx, 272, label);
}

void drawClockView(const frame::Frame& f, const char* const labels[2]) {
    // Time and AM/PM, centred as a pair.
    const uint8_t* time_font = u8g2_font_logisoso92_tn;
    const uint8_t* ampm_font = u8g2_font_helvB18_tf;
    const int16_t  time_w    = textWidth(time_font, f.time);
    const int16_t  ampm_w    = f.ampm[0] ? textWidth(ampm_font, f.ampm) + 8 : 0;
    const int16_t  x0        = (W - time_w - ampm_w) / 2;
    textAt(time_font, x0, 104, f.time);
    if (f.ampm[0]) textAt(ampm_font, x0 + time_w + 8, 104, f.ampm);

    textCentred(u8g2_font_helvR18_tf, W / 2, 140, f.date);

    g_display->drawFastHLine(12, 154, W - 24, GxEPD_BLACK);
    g_display->drawFastVLine(W / 2, 166, 112, GxEPD_BLACK);

    drawReading(f, 0, W / 4, labels[0]);
    drawReading(f, 1, 3 * W / 4, labels[1]);

    // Status line, only when something is wrong. A panel with nothing to
    // report says nothing.
    const char* status = nullptr;
    if (!f.wifi_up) {
        status = "No Wi-Fi";
    } else if (!f.mqtt_up) {
        status = "No MQTT broker";
    }
    if (status != nullptr) textAt(u8g2_font_helvR10_tf, 4, H - 4, status);
}

// ---------------------------------------------------------------------------
// Chart view
// ---------------------------------------------------------------------------
//
//   +---------------------------------------------+
//   | 5:11 PM                      last 12 hours  |
//   +-----------+----+----------------------------+
//   | Maple V.  | 60 |      __/\__                |
//   | 57.2°F    |    |  ___/      \___            |
//   | 81% RH    | 52 |                            |
//   +-----------+----+----------------------------+
//   | Office    | .. |   (same, indoor)           |
//   +-----------+----+----------------------------+

constexpr int16_t HEADER_H = 26;
constexpr int16_t ROW_H    = (H - HEADER_H) / 2;   // 137
constexpr int16_t LEFT_W   = 118;
constexpr int16_t AXIS_R   = 156;                  // right edge of axis labels
constexpr int16_t CHART_X  = 160;
constexpr int16_t CHART_W  = W - CHART_X - 4;      // 236
constexpr size_t  MAX_BUCKETS = CHART_W;

// Maple Valley contributes 48 readings in twelve hours. Downsampled onto 236
// columns every one of them would sit isolated, so it gets one bucket per
// reading and the line is stretched across the width instead.
size_t bucketsFor(size_t i) { return i == 0 ? 48 : MAX_BUCKETS; }

void drawChart(const frame::Frame& f, size_t i, const channel::Channel& ch, int16_t y0) {
    static history::SampleHistory::Bucket buckets[MAX_BUCKETS];
    const size_t n     = bucketsFor(i);
    const size_t valid = ch.history().downsample(CHART_WINDOW_MS, buckets, n);

    const int16_t top    = y0 + 10;
    const int16_t bottom = y0 + ROW_H - 12;
    g_display->drawRect(CHART_X, top, CHART_W, bottom - top + 1, GxEPD_BLACK);

    if (valid == 0) {
        textCentred(u8g2_font_helvR12_tf, CHART_X + CHART_W / 2, (top + bottom) / 2 + 5,
                    "no history yet");
        return;
    }

    float lo = INFINITY, hi = -INFINITY;
    for (size_t k = 0; k < n; ++k) {
        if (!buckets[k].valid) continue;
        const float v = frame::toDisplay(buckets[k].temperature_c, f.fahrenheit);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    // Whole degrees, at least four apart, so a flat line sits mid-chart instead
    // of exaggerating a tenth of a degree into a cliff.
    lo = std::floor(lo);
    hi = std::ceil(hi);
    if (hi - lo < 4.0f) {
        const float mid = (hi + lo) / 2.0f;
        lo = std::floor(mid - 2.0f);
        hi = lo + 4.0f;
    }

    char label[12];
    snprintf(label, sizeof label, "%.0f", hi);
    textRight(u8g2_font_helvR12_tf, AXIS_R, top + 12, label);
    snprintf(label, sizeof label, "%.0f", lo);
    textRight(u8g2_font_helvR12_tf, AXIS_R, bottom, label);

    // Dotted gridlines every three hours, so the eye can find "this morning".
    for (int q = 1; q < 4; ++q) {
        const int16_t x = CHART_X + CHART_W * q / 4;
        for (int16_t y = top + 2; y < bottom; y += 4) g_display->drawPixel(x, y, GxEPD_BLACK);
    }

    const float   span  = hi - lo;
    const int16_t inner = bottom - top - 6;
    int16_t px = -1, py = -1;
    for (size_t k = 0; k < n; ++k) {
        if (!buckets[k].valid) {
            px = -1;  // a gap in the data is a gap in the line
            continue;
        }
        const float   v = frame::toDisplay(buckets[k].temperature_c, f.fahrenheit);
        const int16_t x = CHART_X + 2 +
                          static_cast<int16_t>((CHART_W - 5) * k / (n > 1 ? n - 1 : 1));
        const int16_t y = bottom - 3 - static_cast<int16_t>(inner * (v - lo) / span);
        if (px >= 0) {
            // Two pixels thick: a one-pixel line on e-paper reads as grey.
            g_display->drawLine(px, py, x, y, GxEPD_BLACK);
            g_display->drawLine(px, py + 1, x, y + 1, GxEPD_BLACK);
        } else {
            g_display->fillRect(x - 1, y - 1, 3, 3, GxEPD_BLACK);
        }
        px = x;
        py = y;
    }
}

void drawChartRow(const frame::Frame& f, size_t i, const channel::Channel& ch,
                  const char* label, int16_t y0) {
    textAt(u8g2_font_helvB14_tf, 6, y0 + 28, label);

    char big[24];
    if (f.valid[i]) {
        snprintf(big, sizeof big, "%s%s%s", f.temp[i], DEG, unitSuffix(f.fahrenheit));
    } else {
        snprintf(big, sizeof big, "--");
    }
    textAt(u8g2_font_helvB24_tf, 6, y0 + 68, big);
    if (f.valid[i] && f.stale[i]) {
        strike(4, y0 + 56, textWidth(u8g2_font_helvB24_tf, big) + 4);
    }

    textAt(u8g2_font_helvR14_tf, 6, y0 + 94, f.hum[i]);
    if (f.valid[i] && f.stale[i]) textAt(u8g2_font_helvB14_tf, 6, y0 + 118, "stale");

    drawChart(f, i, ch, y0);
}

void drawChartView(const frame::Frame& f, channel::Channel* const channels[2],
                   const char* const labels[2]) {
    char now[16];
    snprintf(now, sizeof now, "%s %s", f.time, f.ampm);
    textAt(u8g2_font_helvB14_tf, 6, 19, now);
    textRight(u8g2_font_helvR12_tf, W - 6, 19, "last 12 hours");
    g_display->drawFastHLine(0, HEADER_H - 1, W, GxEPD_BLACK);

    drawChartRow(f, 0, *channels[0], labels[0], HEADER_H);
    g_display->drawFastHLine(0, HEADER_H + ROW_H, W, GxEPD_BLACK);
    drawChartRow(f, 1, *channels[1], labels[1], HEADER_H + ROW_H + 1);
}

}  // namespace

bool init() {
    // The panel's supply is switched by a GPIO, and it is off at reset. Nothing
    // the driver sends reaches an unpowered panel, and BUSY floats meanwhile,
    // so this must come first.
    pinMode(board::EPD_POWER, OUTPUT);
    digitalWrite(board::EPD_POWER, HIGH);
    delay(10);

    // GxEPD2 writes CS and DC HIGH before it sets their mode, which core 3.x
    // logs as an error. Both are strapping pins, so make them outputs here,
    // idle high, before anything else touches them.
    pinMode(board::EPD_CS, OUTPUT);
    digitalWrite(board::EPD_CS, HIGH);
    pinMode(board::EPD_DC, OUTPUT);
    digitalWrite(board::EPD_DC, HIGH);

    g_panel = probePanel();
    if (g_panel == Panel::Uc8276) {
        g_display = new (std::nothrow) Uc8276(GxEPD2_420_SE0420NQ04(
            board::EPD_CS, board::EPD_DC, board::EPD_RST, board::EPD_BUSY));
    } else {
        g_display = new (std::nothrow) Ssd1683(GxEPD2_420_GDEY042T81(
            board::EPD_CS, board::EPD_DC, board::EPD_RST, board::EPD_BUSY));
    }
    if (g_display == nullptr) return false;
    Serial.printf("epd: driving it as %s\n", panelName());

    // Explicit pins rather than the S3 defaults: the default SPI.begin() also
    // claims GPIO13 as MISO, which is the microSD card's MISO on this board.
    SPI.begin(board::EPD_SCK, -1, board::EPD_MOSI, board::EPD_CS);
    // 115200 enables GxEPD2's own diagnostics on Serial: busy times per
    // refresh, which is the first thing to look at if the panel misbehaves.
    g_display->init(115200, true, 2, false);
    g_display->setRotation(0);
    g_display->setTextColor(GxEPD_BLACK);

    g_text.begin(*g_display);
    g_text.setFontMode(1);       // transparent background
    g_text.setFontDirection(0);
    g_text.setForegroundColor(GxEPD_BLACK);
    g_text.setBackgroundColor(GxEPD_WHITE);

    return true;
}

Panel panel() { return g_panel; }

const char* panelName() {
    return g_panel == Panel::Uc8276 ? "UC8276C (board V1.2)" : "SSD1683 (board V1.0)";
}

void render(const frame::Frame& f, frame::Refresh kind,
            channel::Channel* const channels[2], const char* const labels[2]) {
    if (kind == frame::Refresh::None) return;

    if (kind == frame::Refresh::Full) {
        g_display->setFullWindow();
    } else {
        g_display->setPartialWindow(0, 0, W, H);
    }

    g_display->firstPage();
    do {
        g_display->fillScreen(GxEPD_WHITE);
        if (f.view == frame::View::Clock) {
            drawClockView(f, labels);
        } else {
            drawChartView(f, channels, labels);
        }
    } while (g_display->nextPage());

    // Turns the booster off between refreshes; the controller keeps both
    // frame buffers, so the next partial refresh still works.
    g_display->powerOff();
}

void message(const char* title, const char* detail) {
    g_display->setPartialWindow(0, 0, W, H);
    g_display->firstPage();
    do {
        g_display->fillScreen(GxEPD_WHITE);
        textCentred(u8g2_font_helvB24_tf, W / 2, H / 2 - 10, title);
        textCentred(u8g2_font_helvR14_tf, W / 2, H / 2 + 26, detail);
    } while (g_display->nextPage());
    g_display->powerOff();
}

}  // namespace ui
