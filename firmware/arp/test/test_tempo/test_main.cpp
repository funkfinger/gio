#include <gtest/gtest.h>
#include <cmath>
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
    // 5 bpm clamped to BPM_MIN (20) → 60000/20 = 3000 ms
    EXPECT_EQ(tempo::bpmToStepMs(5.0f), 3000u);
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

TEST(PotToBpm, HalfwayIsGeometricMean) {
    // Exponential mapping: pot=0.5 → sqrt(BPM_MIN * BPM_MAX)
    float expected = std::sqrt(tempo::BPM_MIN * tempo::BPM_MAX);
    EXPECT_NEAR(tempo::potToBpm(0.5f), expected, 0.01f);
}

TEST(PotToBpm, ConstantRatioPerEqualPotSlice) {
    // Exponential mapping → ratio between consecutive equally-spaced pot
    // positions is constant. Expected ratio per third-turn: (BPM_MAX/BPM_MIN)^(1/3).
    float expected = std::pow(tempo::BPM_MAX / tempo::BPM_MIN, 1.0f / 3.0f);
    float r1 = tempo::potToBpm(1.0f / 3.0f) / tempo::potToBpm(0.0f);
    float r2 = tempo::potToBpm(2.0f / 3.0f) / tempo::potToBpm(1.0f / 3.0f);
    float r3 = tempo::potToBpm(3.0f / 3.0f) / tempo::potToBpm(2.0f / 3.0f);
    EXPECT_NEAR(r1, expected, 0.01f);
    EXPECT_NEAR(r2, expected, 0.01f);
    EXPECT_NEAR(r3, expected, 0.01f);
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
