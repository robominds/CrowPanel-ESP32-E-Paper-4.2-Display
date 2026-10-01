// The two choices a reader makes with the buttons - which view, and which
// units - kept in NVS so they survive a power cut. An e-paper panel goes on
// showing its last picture while unpowered, so a panel that came back in a
// different view or unit from the one it was left showing would look broken.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#pragma once

#include "Frame.h"

namespace settings {

// Loads the saved choices, or the defaults (clock view, Fahrenheit).
void begin();

frame::View view();
bool        fahrenheit();

// Each writes NVS only when the value actually changes.
void setView(frame::View v);
void setFahrenheit(bool f);

}  // namespace settings
