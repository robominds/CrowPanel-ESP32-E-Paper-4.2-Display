// Implementation of the button debouncer.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include "Debouncer.h"

namespace button {

bool Debouncer::update(bool pressed, uint32_t now_ms) {
    if (pressed != raw_) {
        raw_        = pressed;
        changed_ms_ = now_ms;
        return false;
    }

    // Unsigned subtraction, so the settle time survives the millis() rollover.
    if (raw_ == stable_ || now_ms - changed_ms_ < settle_ms_) return false;

    stable_ = raw_;
    return stable_;  // an event on press, none on release
}

}  // namespace button
