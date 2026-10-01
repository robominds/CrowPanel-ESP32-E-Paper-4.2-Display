// Implementation of the update screen.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include "update_screen.h"

#include <Arduino.h>

#include "ui.h"

void UpdateScreen::onTransferStarted(ota::Source) {
    showing_ = true;
    error_   = false;
    Serial.println("ota: transfer started");
    ui::message("Updating firmware", "Do not unplug the panel");
}

void UpdateScreen::onRebooting(ota::Source) {
    Serial.println("ota: rebooting into the new image");
    ui::message("Restarting", "The new firmware starts in a moment");
}

void UpdateScreen::onError(ota::Source, const char* message) {
    Serial.printf("ota: error: %s\n", message);
    showing_     = true;
    error_       = true;
    error_at_ms_ = millis();
    ui::message("Update failed", message);
}

bool UpdateScreen::takeRepaint(uint32_t now_ms) {
    if (!showing_) return false;
    if (error_ && now_ms - error_at_ms_ < ERROR_HOLD_MS) return false;
    // A transfer that started and neither failed nor rebooted cannot get here:
    // the push blocks loop() until one of the two happens.
    showing_ = false;
    error_   = false;
    return true;
}
