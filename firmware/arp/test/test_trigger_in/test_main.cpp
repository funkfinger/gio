#include <gtest/gtest.h>
#include "trigger_in.h"

using trigger_in::Schmitt;

// PlatformIO 6.x doesn't auto-link gtest_main; provide a main here.
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ---------------------------------------------------------------------------
// Initial state — no spurious trigger if input is high at startup
// ---------------------------------------------------------------------------

TEST(SchmittInitial, NotArmedAtConstruction) {
    Schmitt s;
    EXPECT_FALSE(s.armed());
}

TEST(SchmittInitial, IdleHighDoesNotFireOnFirstPoll) {
    // A jack that's normalled high (or a clock that's already high when we
    // boot) must not produce a spurious trigger on the first sample.
    Schmitt s;
    EXPECT_FALSE(s.poll(5.0f));   // way above threshold
    EXPECT_FALSE(s.poll(5.0f));   // still above — still no fire
    EXPECT_FALSE(s.poll(5.0f));
    EXPECT_FALSE(s.armed());
}

TEST(SchmittInitial, ArmsAfterFirstLowSample) {
    Schmitt s;
    EXPECT_FALSE(s.poll(5.0f));  // start high, no fire
    EXPECT_FALSE(s.poll(0.0f));  // drop low → arms
    EXPECT_TRUE(s.armed());
}

// ---------------------------------------------------------------------------
// Standard rising-edge behaviour
// ---------------------------------------------------------------------------

TEST(SchmittRisingEdge, FiresOncePerEdge) {
    Schmitt s;
    // Drop low to arm.
    s.poll(0.0f);
    ASSERT_TRUE(s.armed());

    // Rising edge.
    EXPECT_TRUE(s.poll(5.0f));   // fires
    EXPECT_FALSE(s.poll(5.0f));  // still high → no double-fire
    EXPECT_FALSE(s.poll(5.0f));
}

TEST(SchmittRisingEdge, RepeatsForSquareWave) {
    Schmitt s;
    int fires = 0;
    // 5 cycles of high/low.
    for (int i = 0; i < 5; ++i) {
        s.poll(0.0f);                       // low
        if (s.poll(5.0f)) fires++;          // high → fire
    }
    EXPECT_EQ(fires, 5);
}

TEST(SchmittRisingEdge, FiresAtExactHighThreshold) {
    // Default high threshold is 1.5 V; "exactly equals" should fire.
    Schmitt s;
    s.poll(0.0f);                            // arm
    EXPECT_TRUE(s.poll(1.5f));               // exactly at threshold → fires
}

TEST(SchmittRisingEdge, DoesNotFireBelowHighThreshold) {
    Schmitt s;
    s.poll(0.0f);                            // arm
    EXPECT_FALSE(s.poll(1.49f));             // just below
    EXPECT_TRUE(s.armed());                   // still armed
    EXPECT_TRUE(s.poll(1.5f));               // crossing fires
}

// ---------------------------------------------------------------------------
// Hysteresis — the whole reason this exists
// ---------------------------------------------------------------------------

TEST(SchmittHysteresis, RippleNearHighDoesNotMultiFire) {
    // Signal sits at +3 V with ±200 mV ripple — hovers well above high
    // threshold, never crosses the low threshold. Must fire once total.
    Schmitt s;
    s.poll(0.0f);                            // arm
    int fires = 0;
    for (int i = 0; i < 100; ++i) {
        float v = 3.0f + ((i % 2) ? 0.2f : -0.2f);  // 2.8..3.2 V
        if (s.poll(v)) fires++;
    }
    EXPECT_EQ(fires, 1);
}

TEST(SchmittHysteresis, MustDropBelowLowToReArm) {
    Schmitt s;
    s.poll(0.0f);                            // arm
    EXPECT_TRUE(s.poll(2.0f));               // fires
    // Drop, but only just below high — not yet below low.
    EXPECT_FALSE(s.poll(1.0f));              // 1.0 > 0.5 (low) → does NOT re-arm
    EXPECT_FALSE(s.armed());
    // Still no fire.
    EXPECT_FALSE(s.poll(2.0f));
    // Drop fully below low.
    EXPECT_FALSE(s.poll(0.3f));              // < 0.5 → re-arms
    EXPECT_TRUE(s.armed());
    // Now we can fire again.
    EXPECT_TRUE(s.poll(2.0f));
}

TEST(SchmittHysteresis, ReArmsAtExactLowThreshold) {
    Schmitt s;
    s.poll(0.0f);                            // arm
    s.poll(2.0f);                            // fire
    EXPECT_FALSE(s.armed());
    EXPECT_FALSE(s.poll(0.5f));              // exactly at low → re-arms
    EXPECT_TRUE(s.armed());
}

// ---------------------------------------------------------------------------
// Slow ramp — must fire EXACTLY ONCE on the way up
// ---------------------------------------------------------------------------

TEST(SchmittSlowRamp, MonotonicRiseFiresExactlyOnce) {
    // Simulate a 100-step ramp from 0 V to 5 V (slow, e.g. envelope or
    // attack of an LFO). Only one rising edge worth of trigger should
    // ever fire, no matter how slow the ramp is.
    Schmitt s;
    s.poll(0.0f);                            // arm
    int fires = 0;
    for (int i = 0; i <= 100; ++i) {
        float v = 5.0f * (float)i / 100.0f;  // 0 → 5 V in 100 steps
        if (s.poll(v)) fires++;
    }
    EXPECT_EQ(fires, 1);
}

TEST(SchmittSlowRamp, RampUpThenDownDoesNotMultiFire) {
    Schmitt s;
    s.poll(0.0f);                            // arm
    int fires = 0;
    // Up 0 → 5 V.
    for (int i = 0; i <= 100; ++i) {
        float v = 5.0f * (float)i / 100.0f;
        if (s.poll(v)) fires++;
    }
    // Down 5 → 0 V.
    for (int i = 100; i >= 0; --i) {
        float v = 5.0f * (float)i / 100.0f;
        if (s.poll(v)) fires++;
    }
    EXPECT_EQ(fires, 1);  // only the rising edge counts; falling does not fire
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

TEST(SchmittReset, ClearsArmedState) {
    Schmitt s;
    s.poll(0.0f);                            // arm
    EXPECT_TRUE(s.armed());
    s.reset();
    EXPECT_FALSE(s.armed());
    // After reset, must drop below low again before next fire.
    EXPECT_FALSE(s.poll(2.0f));              // not armed → no fire
    EXPECT_FALSE(s.poll(0.0f));              // re-arms
    EXPECT_TRUE(s.poll(2.0f));               // fires
}

// ---------------------------------------------------------------------------
// Custom thresholds
// ---------------------------------------------------------------------------

TEST(SchmittCustom, UsesCustomThresholds) {
    // TTL-style thresholds: high = 2.0 V, low = 0.8 V.
    Schmitt s(2.0f, 0.8f);
    s.poll(0.0f);                            // arm
    EXPECT_FALSE(s.poll(1.5f));              // below custom high (2.0) → no fire
    EXPECT_TRUE(s.poll(2.0f));               // at custom high → fires
    EXPECT_FALSE(s.poll(1.0f));              // 1.0 > 0.8 (custom low) → no re-arm
    EXPECT_FALSE(s.poll(2.0f));              // not armed
    EXPECT_FALSE(s.poll(0.7f));              // < 0.8 → re-arms
    EXPECT_TRUE(s.poll(2.0f));               // fires again
}

TEST(SchmittCustom, ZeroHysteresisIsValid) {
    // Edge case: high == low (no hysteresis). Useful as a degenerate test;
    // any signal sitting exactly at the threshold could thrash, but a real
    // signal will move through it.
    Schmitt s(2.5f, 2.5f);
    s.poll(0.0f);                            // arm
    EXPECT_TRUE(s.poll(3.0f));               // fires
    EXPECT_FALSE(s.poll(3.0f));              // no double-fire
    EXPECT_FALSE(s.poll(2.5f));              // exactly at low → re-arms (== low_)
    EXPECT_TRUE(s.poll(3.0f));               // fires again
}
