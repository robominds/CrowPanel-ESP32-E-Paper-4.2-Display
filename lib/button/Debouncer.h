// One push button, debounced, reporting a single event per press.
//
// The panel's buttons are plain switches to ground with pull-ups, and the
// contacts bounce for a few milliseconds each way. An e-paper refresh blocks
// the loop for up to four seconds, so a press can also be first seen long
// after it began; that is fine, because the event fires on the first STABLE
// low level, not on an edge that might have been missed.
//
// Free of Arduino headers: the caller reads the pin and supplies millis().
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#pragma once

#include <cstdint>

namespace button {

class Debouncer {
  public:
    explicit Debouncer(uint32_t settle_ms = 30) : settle_ms_(settle_ms) {}

    // `pressed` is the raw level already converted to "pressed" (true when the
    // pin reads low on an active-low button). Returns true exactly once per
    // press, when the press has been stable for settle_ms.
    bool update(bool pressed, uint32_t now_ms);

    // The debounced state.
    bool held() const { return stable_; }

  private:
    uint32_t settle_ms_;
    bool     raw_        = false;
    bool     stable_     = false;
    uint32_t changed_ms_ = 0;
};

}  // namespace button
