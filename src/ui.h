// Everything drawn on the e-paper: the clock view, the chart view, and the
// full-screen messages shown during an over-the-air update.
//
// Drawing is driven entirely by frame::Frame. main.cpp builds a frame every
// loop, asks frame::decide() whether it is worth a refresh, and only then calls
// render(). This module never decides when to draw, only how.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#pragma once

#include <cstdint>

#include "Channel.h"
#include "Frame.h"

namespace ui {

// The chart view's window. Also sizes the channel histories in main.cpp.
constexpr uint32_t CHART_WINDOW_MS = 12UL * 60UL * 60UL * 1000UL;

// The two panel controllers this SKU ships with. See docs/HARDWARE.md.
enum class Panel { Ssd1683, Uc8276 };

// Powers the panel, works out which controller it has, and starts SPI and the
// matching driver. Draws nothing. Returns false only if the driver could not
// be allocated.
bool init();

Panel       panel();
const char* panelName();

// `channels` is {outdoor, indoor}; the chart view reads their histories.
// `labels` are the captions printed under each reading, in the same order.
void render(const frame::Frame& f, frame::Refresh kind,
            channel::Channel* const channels[2], const char* const labels[2]);

// A full-screen message, for over-the-air updates: a large title and one line
// of detail. Always a partial refresh, because a full refresh's three-second
// flash would hold up the transfer it is reporting on.
void message(const char* title, const char* detail);

}  // namespace ui
