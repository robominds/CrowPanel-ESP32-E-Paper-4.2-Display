// Host tests for lib/button.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#include <unity.h>

#include "Debouncer.h"

using button::Debouncer;

void setUp(void) {}
void tearDown(void) {}

static void test_idle_button_never_fires(void) {
    Debouncer b(30);
    for (uint32_t t = 0; t < 1000; t += 5) TEST_ASSERT_FALSE(b.update(false, t));
    TEST_ASSERT_FALSE(b.held());
}

static void test_stable_press_fires_once(void) {
    Debouncer b(30);
    TEST_ASSERT_FALSE(b.update(true, 100));
    TEST_ASSERT_FALSE(b.update(true, 129));
    TEST_ASSERT_TRUE(b.update(true, 130));
    TEST_ASSERT_TRUE(b.held());
    for (uint32_t t = 135; t < 2000; t += 5) TEST_ASSERT_FALSE(b.update(true, t));
}

static void test_bounce_does_not_fire(void) {
    Debouncer b(30);
    uint32_t  t = 100;
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_FALSE(b.update(i % 2 == 0, t));
        t += 4;
    }
    TEST_ASSERT_FALSE(b.held());
}

static void test_release_fires_nothing_and_allows_next_press(void) {
    Debouncer b(30);
    b.update(true, 0);
    TEST_ASSERT_TRUE(b.update(true, 30));
    TEST_ASSERT_FALSE(b.update(false, 100));
    TEST_ASSERT_FALSE(b.update(false, 130));
    TEST_ASSERT_FALSE(b.held());
    TEST_ASSERT_FALSE(b.update(true, 200));
    TEST_ASSERT_TRUE(b.update(true, 230));
}

static void test_press_first_seen_late_still_fires(void) {
    // An e-paper refresh blocked the loop: the first two samples of the press
    // are seconds apart. The press still counts.
    Debouncer b(30);
    TEST_ASSERT_FALSE(b.update(true, 1000));
    TEST_ASSERT_TRUE(b.update(true, 4500));
}

static void test_survives_millis_rollover(void) {
    Debouncer b(30);
    TEST_ASSERT_FALSE(b.update(true, 0xFFFFFFF0u));
    TEST_ASSERT_FALSE(b.update(true, 0xFFFFFFFFu));
    TEST_ASSERT_TRUE(b.update(true, 0x0000000Eu));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_button_never_fires);
    RUN_TEST(test_stable_press_fires_once);
    RUN_TEST(test_bounce_does_not_fire);
    RUN_TEST(test_release_fires_nothing_and_allows_next_press);
    RUN_TEST(test_press_first_seen_late_still_fires);
    RUN_TEST(test_survives_millis_rollover);
    return UNITY_END();
}
