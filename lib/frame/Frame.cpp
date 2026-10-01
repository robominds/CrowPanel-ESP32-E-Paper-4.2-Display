// Implementation of the frame builder and refresh policy.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include "Frame.h"

#include <cstdio>
#include <cstring>

namespace frame {
namespace {

const char* const kWeekdays[7] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                  "Thursday", "Friday", "Saturday"};
const char* const kMonths[12]  = {"January",   "February", "March",    "April",
                                  "May",       "June",     "July",     "August",
                                  "September", "October",  "November", "December"};

// Hand-formatted rather than strftime(): newlib's "%l" pads with a space,
// which would push a single-digit hour off the panel's centre line, and this
// keeps the host tests independent of the C library's locale.
void formatClock(const struct tm& t, Frame& out) {
    int hour12 = t.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(out.time, sizeof out.time, "%d:%02d", hour12, t.tm_min);
    snprintf(out.ampm, sizeof out.ampm, "%s", t.tm_hour < 12 ? "AM" : "PM");

    const int wday = (t.tm_wday >= 0 && t.tm_wday < 7) ? t.tm_wday : 0;
    const int mon  = (t.tm_mon >= 0 && t.tm_mon < 12) ? t.tm_mon : 0;
    snprintf(out.date, sizeof out.date, "%s %d %s %d", kWeekdays[wday], t.tm_mday,
             kMonths[mon], t.tm_year + 1900);
}

void formatSource(const Source& s, bool fahrenheit, size_t i, Frame& out) {
    out.valid[i] = s.valid;
    out.stale[i] = s.stale;
    if (!s.valid) {
        snprintf(out.temp[i], sizeof out.temp[i], "--");
        out.hum[i][0] = '\0';
        return;
    }
    snprintf(out.temp[i], sizeof out.temp[i], "%.1f", toDisplay(s.temp_c, fahrenheit));
    snprintf(out.hum[i], sizeof out.hum[i], "%.0f%% RH", s.hum);
}

// Anything a reader would act on, as opposed to a number drifting.
bool urgentDiffers(const Frame& a, const Frame& b) {
    if (a.fahrenheit != b.fahrenheit || a.have_time != b.have_time) return true;
    if (std::strcmp(a.time, b.time) != 0 || std::strcmp(a.ampm, b.ampm) != 0 ||
        std::strcmp(a.date, b.date) != 0) {
        return true;
    }
    for (size_t i = 0; i < 2; ++i) {
        if (a.valid[i] != b.valid[i] || a.stale[i] != b.stale[i]) return true;
    }
    return false;
}

bool anythingDiffers(const Frame& a, const Frame& b) {
    if (a.view != b.view || urgentDiffers(a, b)) return true;
    if (a.wifi_up != b.wifi_up || a.mqtt_up != b.mqtt_up) return true;
    for (size_t i = 0; i < 2; ++i) {
        if (std::strcmp(a.temp[i], b.temp[i]) != 0) return true;
        if (std::strcmp(a.hum[i], b.hum[i]) != 0) return true;
        if (a.newest_ms[i] != b.newest_ms[i]) return true;
    }
    return false;
}

}  // namespace

float toDisplay(float temp_c, bool fahrenheit) {
    return fahrenheit ? temp_c * 9.0f / 5.0f + 32.0f : temp_c;
}

void build(const Inputs& in, Frame& out) {
    out            = Frame{};
    out.view       = in.view;
    out.fahrenheit = in.fahrenheit;
    out.have_time  = in.have_time;
    out.wifi_up    = in.wifi_up;
    out.mqtt_up    = in.mqtt_up;

    if (in.have_time) {
        formatClock(in.local, out);
    } else {
        snprintf(out.time, sizeof out.time, "--:--");
        snprintf(out.date, sizeof out.date, "Waiting for the time");
    }

    formatSource(in.outdoor, in.fahrenheit, 0, out);
    formatSource(in.indoor, in.fahrenheit, 1, out);

    if (in.view == View::Charts) {
        out.newest_ms[0] = in.newest_ms[0];
        out.newest_ms[1] = in.newest_ms[1];
    }
}

Refresh decide(const Frame& last, const Frame& next, bool first,
               uint32_t ms_since_draw, uint16_t partials, const Policy& policy) {
    if (first) return Refresh::Full;
    if (!anythingDiffers(last, next)) return Refresh::None;
    if (last.view != next.view) return Refresh::Full;

    const bool due = urgentDiffers(last, next) || ms_since_draw >= policy.min_interval_ms;
    if (!due) return Refresh::None;

    return partials >= policy.full_every ? Refresh::Full : Refresh::Partial;
}

}  // namespace frame
