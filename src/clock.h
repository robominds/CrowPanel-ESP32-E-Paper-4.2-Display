// Wall-clock time, from the network.
//
// Unlike the CrowPanel Advance 7.0, this board has no real-time clock chip, so
// after every power-up the time is unknown until SNTP answers. The clock view
// prints "--:--" until then rather than a 1970 it made up. Once synced, the
// ESP32's own timer keeps time to within a second or two a day, and SNTP
// corrects it every hour.
//
// Everything here works in LOCAL time, because the only consumer is a person
// reading the panel.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#pragma once

#include <ctime>

namespace clock_source {

// Sets the timezone. Call once, before anything wants the time.
void begin(const char* tz);

// Call every loop. Starts SNTP the first time the network is up.
void poll(bool network_up);

// True when the system clock holds a plausible wall-clock time.
bool hasTime();

}  // namespace clock_source
