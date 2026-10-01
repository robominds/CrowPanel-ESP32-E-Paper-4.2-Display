// Implementation of the wall-clock source.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include "clock.h"

#include <Arduino.h>
#include <esp_sntp.h>

namespace clock_source {
namespace {

// Anything before this is not a real wall-clock reading. Used to tell a synced
// clock from the 1970 the system starts at.
constexpr time_t PLAUSIBLE_EPOCH = 1767225600;  // 2026-01-01

bool g_started = false;
bool g_logged  = false;

}  // namespace

void begin(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
}

void poll(bool network_up) {
    if (!g_started && network_up) {
        // lwIP's SNTP client keeps polling on its own once started (hourly by
        // default) and survives Wi-Fi drops, so this only ever runs once.
        configTzTime(getenv("TZ"), "pool.ntp.org", "time.nist.gov");
        g_started = true;
        Serial.println("clock: asking the network for the time");
    }

    if (!g_logged && hasTime()) {
        g_logged         = true;
        const time_t now = time(nullptr);
        struct tm    lt;
        localtime_r(&now, &lt);
        Serial.printf("clock: network says %04d-%02d-%02d %02d:%02d:%02d\n",
                      lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                      lt.tm_min, lt.tm_sec);
    }
}

bool hasTime() { return time(nullptr) >= PLAUSIBLE_EPOCH; }

}  // namespace clock_source
