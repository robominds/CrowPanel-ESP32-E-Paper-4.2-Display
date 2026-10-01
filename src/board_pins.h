// Every GPIO this firmware uses on the Elecrow CrowPanel ESP32 E-Paper HMI
// 4.2" (SKU DIE07300S), named, with where each value came from.
//
// Source for every pin: Elecrow's own Eagle schematic, net names read straight
// out of the .sch XML, cross-checked against their example code and the PCB
// silkscreen. See docs/HARDWARE.md for the full reference and its citations.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#pragma once

#include <cstdint>

namespace board {

// ---------------------------------------------------------------------------
// E-paper panel, 400 x 300, black/white. Write-only SPI: there is no MISO.
// ---------------------------------------------------------------------------
constexpr int16_t EPD_WIDTH  = 400;
constexpr int16_t EPD_HEIGHT = 300;

constexpr uint8_t EPD_SCK  = 12;   // IO12_SPI_CLK
constexpr uint8_t EPD_MOSI = 11;   // IO11_SPI_MOSI
constexpr uint8_t EPD_CS   = 45;   // IO45_CS   - strapping pin (VDD_SPI); no pull-up
constexpr uint8_t EPD_DC   = 46;   // IO46_D/C  - strapping pin (ROM log)
constexpr uint8_t EPD_RST  = 47;   // IO47_RES
constexpr uint8_t EPD_BUSY = 48;   // IO48_BUSY - polarity depends on the controller

// HIGH connects the panel's ground through a 2N7002 (Q9). Off at reset, and
// nothing reaches the panel until it is driven HIGH.
constexpr uint8_t EPD_POWER = 7;   // IO7_LCD_3.3_CTL

// ---------------------------------------------------------------------------
// Buttons. All active LOW with external 4.7 k pull-ups, no RC debounce.
// ---------------------------------------------------------------------------
constexpr uint8_t BTN_HOME = 2;    // K3, silkscreen MENU
constexpr uint8_t BTN_EXIT = 1;    // K4
constexpr uint8_t BTN_UP   = 6;    // K5 rotary switch, up
constexpr uint8_t BTN_DOWN = 4;    // K5 rotary switch, down
constexpr uint8_t BTN_OK   = 5;    // K5 rotary switch, pressed

// ---------------------------------------------------------------------------
// Red LED D1, active HIGH. Elecrow's code calls GPIO41 "power control"; on the
// schematic it drives only this LED. Kept off: the panel is read in a dark
// room too.
// ---------------------------------------------------------------------------
constexpr uint8_t LED = 41;

}  // namespace board
