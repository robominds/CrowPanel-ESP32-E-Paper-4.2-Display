// What the e-paper shows while a firmware update arrives over Wi-Fi.
//
// esp32-ota-kit reports through an ota::Observer and draws nothing itself. A
// push blocks loop() until it ends, so each message is drawn from inside the
// observer call. Progress is deliberately NOT drawn: every partial refresh
// stalls the transfer for half a second, and an e-paper progress bar is a
// slower upload for no benefit. One message at the start, one at the end.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#pragma once

#include <cstdint>

#include <ota.h>

class UpdateScreen : public ota::Observer {
  public:
    void onTransferStarted(ota::Source source) override;
    void onRebooting(ota::Source source) override;
    void onError(ota::Source source, const char* message) override;

    // True once an error has been on screen for ERROR_HOLD_MS, or never: the
    // caller should then repaint the normal view with a full refresh. Clears
    // itself when it returns true.
    bool takeRepaint(uint32_t now_ms);

    // True while an update message owns the panel.
    bool showing() const { return showing_; }

    static constexpr uint32_t ERROR_HOLD_MS = 8000;

  private:
    bool     showing_     = false;
    bool     error_       = false;
    uint32_t error_at_ms_ = 0;
};
