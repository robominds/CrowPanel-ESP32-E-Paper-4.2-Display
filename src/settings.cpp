// Implementation of the persisted settings.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include "settings.h"

#include <Preferences.h>

namespace settings {
namespace {

constexpr char NAMESPACE[] = "display";

Preferences g_prefs;
frame::View g_view       = frame::View::Clock;
bool        g_fahrenheit = true;

}  // namespace

void begin() {
    g_prefs.begin(NAMESPACE, false);
    g_view       = g_prefs.getUChar("view", 0) == 1 ? frame::View::Charts
                                                   : frame::View::Clock;
    g_fahrenheit = g_prefs.getBool("fahrenheit", true);
}

frame::View view() { return g_view; }
bool        fahrenheit() { return g_fahrenheit; }

void setView(frame::View v) {
    if (v == g_view) return;
    g_view = v;
    g_prefs.putUChar("view", static_cast<uint8_t>(v));
}

void setFahrenheit(bool f) {
    if (f == g_fahrenheit) return;
    g_fahrenheit = f;
    g_prefs.putBool("fahrenheit", f);
}

}  // namespace settings
