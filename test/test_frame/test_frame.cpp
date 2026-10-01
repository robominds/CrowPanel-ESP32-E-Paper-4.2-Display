// Host tests for lib/frame: the text each view prints and the refresh policy
// that decides when the e-paper is worth touching.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include <unity.h>

#include <cstring>

#include "Frame.h"

using frame::Frame;
using frame::Inputs;
using frame::Policy;
using frame::Refresh;
using frame::View;

void setUp(void) {}
void tearDown(void) {}

static struct tm at(int year, int mon, int mday, int hour, int min, int wday) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon  = mon - 1;
    t.tm_mday = mday;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_wday = wday;
    return t;
}

// A typical afternoon: clock known, both sources live.
static Inputs live() {
    Inputs in;
    in.have_time = true;
    in.local     = at(2026, 9, 30, 17, 11, 3);  // Wednesday
    in.outdoor   = {true, false, 14.0f, 81.0f};
    in.indoor    = {true, false, 21.6f, 44.4f};
    in.wifi_up   = true;
    in.mqtt_up   = true;
    return in;
}

static Frame built(const Inputs& in) {
    Frame f;
    frame::build(in, f);
    return f;
}

// ---------------------------------------------------------------------------
// Units
// ---------------------------------------------------------------------------

static void test_to_display_converts_only_when_asked(void) {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 21.6f, frame::toDisplay(21.6f, false));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 32.0f, frame::toDisplay(0.0f, true));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 212.0f, frame::toDisplay(100.0f, true));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -40.0f, frame::toDisplay(-40.0f, true));
}

// ---------------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------------

static void test_clock_is_twelve_hour_without_leading_space(void) {
    Frame f = built(live());
    TEST_ASSERT_EQUAL_STRING("5:11", f.time);
    TEST_ASSERT_EQUAL_STRING("PM", f.ampm);
}

static void test_clock_at_noon_and_midnight(void) {
    Inputs in = live();
    in.local  = at(2026, 9, 30, 0, 5, 3);
    Frame f   = built(in);
    TEST_ASSERT_EQUAL_STRING("12:05", f.time);
    TEST_ASSERT_EQUAL_STRING("AM", f.ampm);

    in.local = at(2026, 9, 30, 12, 0, 3);
    f        = built(in);
    TEST_ASSERT_EQUAL_STRING("12:00", f.time);
    TEST_ASSERT_EQUAL_STRING("PM", f.ampm);
}

static void test_date_is_spelled_out(void) {
    Frame f = built(live());
    TEST_ASSERT_EQUAL_STRING("Wednesday 30 September 2026", f.date);
}

static void test_no_clock_shows_dashes_not_1970(void) {
    Inputs in    = live();
    in.have_time = false;
    Frame f      = built(in);
    TEST_ASSERT_EQUAL_STRING("--:--", f.time);
    TEST_ASSERT_EQUAL_STRING("", f.ampm);
    TEST_ASSERT_EQUAL_STRING("Waiting for the time", f.date);
}

static void test_temperatures_have_one_decimal_in_chosen_units(void) {
    Inputs in     = live();
    in.fahrenheit = false;
    Frame f       = built(in);
    TEST_ASSERT_EQUAL_STRING("14.0", f.temp[0]);
    TEST_ASSERT_EQUAL_STRING("21.6", f.temp[1]);

    in.fahrenheit = true;
    f             = built(in);
    TEST_ASSERT_EQUAL_STRING("57.2", f.temp[0]);
    TEST_ASSERT_EQUAL_STRING("70.9", f.temp[1]);
}

static void test_humidity_is_whole_percent(void) {
    Frame f = built(live());
    TEST_ASSERT_EQUAL_STRING("81% RH", f.hum[0]);
    TEST_ASSERT_EQUAL_STRING("44% RH", f.hum[1]);
}

static void test_no_reading_yet_shows_dashes(void) {
    Inputs in = live();
    in.indoor = {};
    Frame f   = built(in);
    TEST_ASSERT_FALSE(f.valid[1]);
    TEST_ASSERT_EQUAL_STRING("--", f.temp[1]);
    TEST_ASSERT_EQUAL_STRING("", f.hum[1]);
}

static void test_stale_reading_keeps_its_number(void) {
    // A stale value is still the last thing known; the view marks it rather
    // than hiding it.
    Inputs in       = live();
    in.indoor.stale = true;
    Frame f         = built(in);
    TEST_ASSERT_TRUE(f.stale[1]);
    TEST_ASSERT_EQUAL_STRING("70.9", f.temp[1]);
}

static void test_history_timestamps_only_matter_in_chart_view(void) {
    Inputs in       = live();
    in.newest_ms[0] = 1234;
    in.newest_ms[1] = 5678;
    Frame clock     = built(in);
    TEST_ASSERT_EQUAL_UINT32(0, clock.newest_ms[0]);
    TEST_ASSERT_EQUAL_UINT32(0, clock.newest_ms[1]);

    in.view      = View::Charts;
    Frame charts = built(in);
    TEST_ASSERT_EQUAL_UINT32(1234, charts.newest_ms[0]);
    TEST_ASSERT_EQUAL_UINT32(5678, charts.newest_ms[1]);
}

// ---------------------------------------------------------------------------
// Refresh policy
// ---------------------------------------------------------------------------

static const Policy kPolicy;

static void test_first_draw_is_full(void) {
    Frame f = built(live());
    TEST_ASSERT_EQUAL(Refresh::Full, frame::decide(f, f, true, 0, 0, kPolicy));
}

static void test_identical_frame_needs_nothing(void) {
    Frame f = built(live());
    TEST_ASSERT_EQUAL(Refresh::None,
                      frame::decide(f, f, false, 600000, 0, kPolicy));
}

static void test_new_minute_is_partial_immediately(void) {
    Inputs in = live();
    Frame a   = built(in);
    in.local  = at(2026, 9, 30, 17, 12, 3);
    Frame b   = built(in);
    TEST_ASSERT_EQUAL(Refresh::Partial, frame::decide(a, b, false, 100, 3, kPolicy));
}

static void test_number_change_waits_for_the_interval(void) {
    Inputs in       = live();
    Frame a         = built(in);
    in.indoor.temp_c = 21.7f;
    Frame b         = built(in);
    TEST_ASSERT_EQUAL(Refresh::None, frame::decide(a, b, false, 59999, 0, kPolicy));
    TEST_ASSERT_EQUAL(Refresh::Partial,
                      frame::decide(a, b, false, 60000, 0, kPolicy));
}

static void test_connectivity_change_waits_for_the_interval(void) {
    Inputs in  = live();
    Frame a    = built(in);
    in.mqtt_up = false;
    Frame b    = built(in);
    TEST_ASSERT_EQUAL(Refresh::None, frame::decide(a, b, false, 10, 0, kPolicy));
}

static void test_going_stale_is_shown_immediately(void) {
    Inputs in       = live();
    Frame a         = built(in);
    in.indoor.stale = true;
    Frame b         = built(in);
    TEST_ASSERT_EQUAL(Refresh::Partial, frame::decide(a, b, false, 10, 0, kPolicy));
}

static void test_first_reading_is_shown_immediately(void) {
    Inputs in = live();
    in.indoor = {};
    Frame a   = built(in);
    in.indoor = {true, false, 21.6f, 44.4f};
    Frame b   = built(in);
    TEST_ASSERT_EQUAL(Refresh::Partial, frame::decide(a, b, false, 10, 0, kPolicy));
}

static void test_unit_toggle_is_shown_immediately(void) {
    Inputs in     = live();
    Frame a       = built(in);
    in.fahrenheit = false;
    Frame b       = built(in);
    TEST_ASSERT_EQUAL(Refresh::Partial, frame::decide(a, b, false, 10, 0, kPolicy));
}

static void test_acquiring_the_clock_is_shown_immediately(void) {
    Inputs in    = live();
    in.have_time = false;
    Frame a      = built(in);
    in.have_time = true;
    Frame b      = built(in);
    TEST_ASSERT_EQUAL(Refresh::Partial, frame::decide(a, b, false, 10, 0, kPolicy));
}

static void test_view_change_is_full(void) {
    Inputs in = live();
    Frame a   = built(in);
    in.view   = View::Charts;
    Frame b   = built(in);
    TEST_ASSERT_EQUAL(Refresh::Full, frame::decide(a, b, false, 10, 0, kPolicy));
}

static void test_every_nth_refresh_is_full(void) {
    Inputs in = live();
    Frame a   = built(in);
    in.local  = at(2026, 9, 30, 17, 12, 3);
    Frame b   = built(in);
    TEST_ASSERT_EQUAL(Refresh::Partial,
                      frame::decide(a, b, false, 60000, kPolicy.full_every - 1, kPolicy));
    TEST_ASSERT_EQUAL(Refresh::Full,
                      frame::decide(a, b, false, 60000, kPolicy.full_every, kPolicy));
}

static void test_due_full_refresh_still_waits_for_a_change(void) {
    // Ghosting is only worth clearing when something is being drawn anyway; a
    // panel nobody has changed does not flash on its own.
    Frame f = built(live());
    TEST_ASSERT_EQUAL(Refresh::None,
                      frame::decide(f, f, false, 600000, 1000, kPolicy));
}

static void test_new_chart_sample_waits_for_the_interval(void) {
    Inputs in       = live();
    in.view         = View::Charts;
    in.newest_ms[1] = 1000;
    Frame a         = built(in);
    in.newest_ms[1] = 11000;
    Frame b         = built(in);
    TEST_ASSERT_EQUAL(Refresh::None, frame::decide(a, b, false, 10000, 0, kPolicy));
    TEST_ASSERT_EQUAL(Refresh::Partial,
                      frame::decide(a, b, false, 60000, 0, kPolicy));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_to_display_converts_only_when_asked);

    RUN_TEST(test_clock_is_twelve_hour_without_leading_space);
    RUN_TEST(test_clock_at_noon_and_midnight);
    RUN_TEST(test_date_is_spelled_out);
    RUN_TEST(test_no_clock_shows_dashes_not_1970);
    RUN_TEST(test_temperatures_have_one_decimal_in_chosen_units);
    RUN_TEST(test_humidity_is_whole_percent);
    RUN_TEST(test_no_reading_yet_shows_dashes);
    RUN_TEST(test_stale_reading_keeps_its_number);
    RUN_TEST(test_history_timestamps_only_matter_in_chart_view);

    RUN_TEST(test_first_draw_is_full);
    RUN_TEST(test_identical_frame_needs_nothing);
    RUN_TEST(test_new_minute_is_partial_immediately);
    RUN_TEST(test_number_change_waits_for_the_interval);
    RUN_TEST(test_connectivity_change_waits_for_the_interval);
    RUN_TEST(test_going_stale_is_shown_immediately);
    RUN_TEST(test_first_reading_is_shown_immediately);
    RUN_TEST(test_unit_toggle_is_shown_immediately);
    RUN_TEST(test_acquiring_the_clock_is_shown_immediately);
    RUN_TEST(test_view_change_is_full);
    RUN_TEST(test_every_nth_refresh_is_full);
    RUN_TEST(test_due_full_refresh_still_waits_for_a_change);
    RUN_TEST(test_new_chart_sample_waits_for_the_interval);

    return UNITY_END();
}
