// Temperature clock for the Elecrow CrowPanel ESP32 E-Paper HMI 4.2".
//
// The time, the outdoor temperature for Maple Valley from Open-Meteo, and the
// indoor temperature from Mark's office via the MQTT broker - the board has no
// sensor of its own. A second view charts both over twelve hours. The buttons
// switch views and units.
//
// The firmware is built around one fact: an e-paper refresh is expensive. The
// loop gathers data continuously, builds a text-only description of what the
// panel should say (frame::Frame), and touches the panel only when that
// description has changed enough to deserve it. See lib/frame.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include <Arduino.h>
#include <ota.h>

#include <new>

#include "Channel.h"
#include "Debouncer.h"
#include "Frame.h"
#include "board_pins.h"
#include "clock.h"
#include "net.h"
#include "settings.h"
#include "source_mqtt.h"
#include "source_weather.h"
#include "ui.h"
#include "update_screen.h"

namespace {

// Expected publish intervals, used only to size the ring buffers. The office
// DHT publishes every ten seconds.
constexpr uint32_t INDOOR_INTERVAL_MS  = 10UL * 1000UL;
constexpr uint32_t WEATHER_INTERVAL_MS = 15UL * 60UL * 1000UL;

// Per-channel staleness. The office publishes every ten seconds, so a minute
// of silence means something broke. Maple Valley updates every fifteen
// minutes, so a minute of silence is normal.
constexpr uint32_t INDOOR_STALE_MS  = 60UL * 1000UL;
constexpr uint32_t WEATHER_STALE_MS = 45UL * 60UL * 1000UL;

// Capacity is the window divided by the interval, plus slack so the chart's
// window is always fully covered rather than starved at the left edge.
constexpr size_t INDOOR_CAPACITY  = (ui::CHART_WINDOW_MS / INDOOR_INTERVAL_MS) + 64;
constexpr size_t WEATHER_CAPACITY = (ui::CHART_WINDOW_MS / WEATHER_INTERVAL_MS) + 8;

// [0] outdoor, [1] indoor. Built in setup(), not at file scope: the indoor
// history is about 52 KB, and an allocation failure during static
// initialisation reboots the board before Serial can say why.
channel::Channel* g_channels[2] = {nullptr, nullptr};
const char* const g_labels[2]   = {OUTDOOR_LABEL, INDOOR_LABEL};

// Local time. Maple Valley is America/Los_Angeles. A POSIX rule rather than a
// zone name so no timezone database has to be carried.
#ifndef DISPLAY_TZ
#define DISPLAY_TZ "PST8PDT,M3.2.0,M11.1.0"
#endif

UpdateScreen g_update_screen;

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------

struct Button {
    uint8_t           pin;
    button::Debouncer debouncer;
};

enum ButtonId { HOME, EXIT, UP, DOWN, OK, BUTTON_COUNT };

Button g_buttons[BUTTON_COUNT] = {
    {board::BTN_HOME, button::Debouncer()},
    {board::BTN_EXIT, button::Debouncer()},
    {board::BTN_UP, button::Debouncer()},
    {board::BTN_DOWN, button::Debouncer()},
    {board::BTN_OK, button::Debouncer()},
};

// Set by the OK button: the next refresh is a full one, to clear ghosting on
// demand.
bool g_force_full = false;

void pollButtons() {
    const uint32_t now = millis();
    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        // Active low: switches to ground against the internal pull-ups.
        const bool pressed = digitalRead(g_buttons[i].pin) == LOW;
        if (!g_buttons[i].debouncer.update(pressed, now)) continue;

        switch (i) {
            case HOME:
            case UP:
            case DOWN:
                settings::setView(settings::view() == frame::View::Clock
                                      ? frame::View::Charts
                                      : frame::View::Clock);
                Serial.printf("view: %s\n",
                              settings::view() == frame::View::Clock ? "clock" : "charts");
                break;
            case EXIT:
                settings::setFahrenheit(!settings::fahrenheit());
                Serial.printf("units: %s\n", settings::fahrenheit() ? "F" : "C");
                break;
            case OK:
                g_force_full = true;
                Serial.println("refresh: full, on request");
                break;
            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

frame::Source sourceOf(const channel::Channel& ch, uint32_t now) {
    const channel::Reading& r = ch.latest();
    return frame::Source{r.valid, ch.isStale(now), r.temperature_c, r.humidity_pct};
}

void pollDisplay() {
    static frame::Frame  last;
    static bool          first         = true;
    static uint32_t      last_draw_ms  = 0;
    static uint16_t      partials      = 0;
    static const frame::Policy policy;

    const uint32_t now = millis();

    if (g_update_screen.takeRepaint(now)) first = true;
    if (g_update_screen.showing()) return;  // an update error is on screen

    frame::Inputs in;
    in.view       = settings::view();
    in.fahrenheit = settings::fahrenheit();
    in.have_time  = clock_source::hasTime();
    if (in.have_time) {
        const time_t t = time(nullptr);
        localtime_r(&t, &in.local);
    }
    in.outdoor = sourceOf(*g_channels[0], now);
    in.indoor  = sourceOf(*g_channels[1], now);
    in.wifi_up = net::connected();
    in.mqtt_up = source_mqtt::connected();
    for (size_t i = 0; i < 2; ++i) {
        const channel::Reading& r = g_channels[i]->latest();
        in.newest_ms[i]           = r.valid ? r.t_ms : 0;
    }

    frame::Frame next;
    frame::build(in, next);

    frame::Refresh kind =
        frame::decide(last, next, first, now - last_draw_ms, partials, policy);
    if (g_force_full) {
        kind         = frame::Refresh::Full;
        g_force_full = false;
    }
    if (kind == frame::Refresh::None) return;

    const uint32_t started = millis();
    ui::render(next, kind, g_channels, g_labels);
    Serial.printf("refresh: %s in %lu ms, %s\n",
                  kind == frame::Refresh::Full ? "full" : "partial",
                  static_cast<unsigned long>(millis() - started), next.time);

    last         = next;
    first        = false;
    last_draw_ms = now;
    partials     = (kind == frame::Refresh::Full) ? 0 : partials + 1;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    // Serial reaches the host through a CH340 bridge, not native USB. Nothing
    // to wait for, so no while(!Serial) here - that would hang forever.
    delay(200);
    Serial.printf("\nCrowPanel E-Paper 4.2 temperature clock %s\n", FW_VERSION);

    g_channels[0] = new (std::nothrow)
        channel::Channel(OUTDOOR_LABEL, WEATHER_STALE_MS, WEATHER_CAPACITY);
    g_channels[1] = new (std::nothrow)
        channel::Channel(INDOOR_LABEL, INDOOR_STALE_MS, INDOOR_CAPACITY);
    if (g_channels[0] == nullptr || g_channels[1] == nullptr) {
        Serial.println("FATAL: out of memory allocating the channel histories");
        while (true) delay(1000);
    }

    for (Button& b : g_buttons) pinMode(b.pin, INPUT_PULLUP);
    pinMode(board::LED, OUTPUT);
    digitalWrite(board::LED, LOW);

    settings::begin();
    clock_source::begin(DISPLAY_TZ);

    if (!ui::init()) {
        Serial.println("FATAL: the e-paper panel did not respond");
        while (true) delay(1000);
    }

    // Push updates and rollback. Before Wi-Fi, which starts push. Pull
    // updates stay off: no manifest_url.
    ota::Config ota_config;
    ota_config.hostname        = OTA_HOSTNAME;
    ota_config.push_password   = OTA_PASSWORD;
    ota_config.running_version = FW_VERSION;
    ota::begin(ota_config, g_update_screen);

    net::begin();
    source_mqtt::begin(*g_channels[1]);
    source_weather::begin(*g_channels[0]);

    // First picture straight away: the clock and readings show dashes until
    // the network fills them in, which says "alive, waiting" rather than
    // leaving whatever the panel showed before power-up.
    pollDisplay();

    Serial.println("ready");
}

void loop() {
    net::poll();
    // A push blocks here until it finishes; MQTT may drop meanwhile and
    // reconnects on its own if the update fails.
    ota::setNetworkUp(net::connected());
    ota::poll();
    clock_source::poll(net::connected());
    source_mqtt::poll();
    source_weather::poll();

    pollButtons();
    pollDisplay();

    delay(10);
}
