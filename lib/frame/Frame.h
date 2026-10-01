// What the e-paper should show, as text, and whether it is worth repainting.
//
// An e-paper panel is not a screen that repaints for free. A partial refresh
// takes about half a second and leaves a faint ghost; a full refresh takes
// three or four seconds and flashes the whole panel black and white. So the
// firmware never draws "every loop". Instead it builds a Frame - every string
// the current view would print - and compares it with the Frame last drawn.
// Only a real difference reaches the panel, and decide() says which kind of
// refresh that difference deserves.
//
// Free of Arduino, ESP-IDF and GxEPD2 headers so both the formatting and the
// refresh policy are tested on the host.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>

namespace frame {

enum class View : uint8_t { Clock = 0, Charts = 1 };

// One temperature source, as the display needs it. Plain values rather than a
// channel::Channel so this library depends on nothing.
struct Source {
    bool  valid  = false;   // false until the first reading arrives
    bool  stale  = true;    // the source has gone quiet
    float temp_c = 0.0f;
    float hum    = 0.0f;
};

struct Inputs {
    View      view       = View::Clock;
    bool      fahrenheit = true;
    bool      have_time  = false;  // the wall clock is plausible
    struct tm local      = {};     // meaningful only when have_time
    Source    outdoor;
    Source    indoor;
    bool      wifi_up    = false;
    bool      mqtt_up    = false;

    // Newest history timestamp per channel, outdoor then indoor. Only the
    // chart view prints history, so only it looks at these.
    uint32_t  newest_ms[2] = {0, 0};
};

// Index 0 is outdoor, 1 is indoor, everywhere below.
struct Frame {
    View    view       = View::Clock;
    bool    fahrenheit = true;
    bool    have_time  = false;

    char    time[8]  = "";   // "1:05" or "12:34", or "--:--" without a clock
    char    ampm[3]  = "";   // "AM" / "PM", empty without a clock
    char    date[40] = "";   // "Wednesday 30 September 2026"

    bool    valid[2] = {false, false};
    bool    stale[2] = {true, true};
    char    temp[2][12] = {"", ""};  // "72.4", or "--" before the first reading
    char    hum[2][12]  = {"", ""};  // "55% RH", or "" before the first reading

    bool    wifi_up = false;
    bool    mqtt_up = false;

    // Chart view only; zero in the clock view so new samples do not count as
    // a change there.
    uint32_t newest_ms[2] = {0, 0};
};

// Celsius in, display units out.
float toDisplay(float temp_c, bool fahrenheit);

// Fills `out` from `in`. Pure: the same inputs always produce the same frame.
void build(const Inputs& in, Frame& out);

enum class Refresh : uint8_t { None, Partial, Full };

struct Policy {
    // A partial refresh leaves a little residue each time; a full refresh
    // clears it. Thirty partials at one a minute is a full refresh every half
    // hour, which keeps the panel clean without flashing it often.
    uint16_t full_every = 30;

    // Changes that are only new numbers - a temperature moving by a tenth -
    // wait at least this long since the last refresh. The office publishes
    // every ten seconds; without this the panel would refresh six times a
    // minute for nothing anyone is watching.
    uint32_t min_interval_ms = 60000;
};

// What to do about `next`, given `last` was drawn `ms_since_draw` ago and
// `partials` partial refreshes have happened since the last full one. `first`
// means nothing has been drawn since power-up, so the panel's contents are
// unknown.
//
//   - nothing differs                          -> None
//   - first draw, or the view changed          -> Full
//   - due for a full refresh                   -> Full
//   - the clock or date changed, or a state
//     a reader would act on (stale, first
//     reading, units, clock acquired)          -> Partial, now
//   - only numbers or connectivity changed     -> Partial, once min_interval
//                                                 has passed, else None
Refresh decide(const Frame& last, const Frame& next, bool first,
               uint32_t ms_since_draw, uint16_t partials, const Policy& policy);

}  // namespace frame
