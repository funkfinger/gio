#include <gtest/gtest.h>
#include "tempo.h"

// ---------------------------------------------------------------------------
// bpmToStepMs
// ---------------------------------------------------------------------------

TEST(BpmToStepMs, SixtyBpmQuarterNoteIsOneSecond) {
    EXPECT_EQ(tempo::bpmToStepMs(60.0f), 1000u);
}

TEST(BpmToStepMs, OneTwentyBpmQuarterIsHalfSecond) {
    EXPECT_EQ(tempo::bpmToStepMs(120.0f), 500u);
}

TEST(BpmToStepMs, OneTwentyBpmEighthNotes) {
    EXPECT_EQ(tempo::bpmToStepMs(120.0f, 2), 250u);
}

TEST(BpmToStepMs, OneTwentyBpmSixteenthNotes) {
    EXPECT_EQ(tempo::bpmToStepMs(120.0f, 4), 125u);
}

TEST(BpmToStepMs, ClampsBelowMin) {
    // 10 bpm clamped to 30 → 60000/30 = 2000 ms
    EXPECT_EQ(tempo::bpmToStepMs(10.0f), 2000u);
}

TEST(BpmToStepMs, ClampsAboveMax) {
    // 1000 bpm clamped to 300 → 60000/300 = 200 ms
    EXPECT_EQ(tempo::bpmToStepMs(1000.0f), 200u);
}

TEST(BpmToStepMs, SubZeroTreatedAsOne) {
    EXPECT_EQ(tempo::bpmToStepMs(120.0f, 0), 500u);
}

// ---------------------------------------------------------------------------
// potToBpm
// ---------------------------------------------------------------------------

TEST(PotToBpm, ZeroIsMin) {
    EXPECT_FLOAT_EQ(tempo::potToBpm(0.0f), tempo::BPM_MIN);
}

TEST(PotToBpm, OneIsMax) {
    EXPECT_FLOAT_EQ(tempo::potToBpm(1.0f), tempo::BPM_MAX);
}

TEST(PotToBpm, HalfwayIsMidpoint) {
    EXPECT_FLOAT_EQ(tempo::potToBpm(0.5f), (tempo::BPM_MIN + tempo::BPM_MAX) * 0.5f);
}

TEST(PotToBpm, ClampsBelowZero) {
    EXPECT_FLOAT_EQ(tempo::potToBpm(-0.5f), tempo::BPM_MIN);
}

TEST(PotToBpm, ClampsAboveOne) {
    EXPECT_FLOAT_EQ(tempo::potToBpm(2.0f), tempo::BPM_MAX);
}

// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
