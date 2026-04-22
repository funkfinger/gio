#include <gtest/gtest.h>
#include "cv.h"
#include "scales.h"

// ---------------------------------------------------------------------------
// Boundary cases — voltages outside the Eurorack range
// ---------------------------------------------------------------------------

TEST(CvVoltsToTranspose, ZeroVoltsIsZeroSemitones) {
    for (auto s : {Scale::Major, Scale::NaturalMinor, Scale::PentatonicMajor,
                   Scale::PentatonicMinor, Scale::Dorian, Scale::Chromatic}) {
        EXPECT_EQ(cvVoltsToTranspose(0.0f, s), 0) << "scale=" << (int)s;
    }
}

TEST(CvVoltsToTranspose, NegativeVoltsClampToZero) {
    EXPECT_EQ(cvVoltsToTranspose(-1.0f,  Scale::Major), 0);
    EXPECT_EQ(cvVoltsToTranspose(-0.01f, Scale::Major), 0);
    EXPECT_EQ(cvVoltsToTranspose(-0.5f,  Scale::Chromatic), 0);
}

TEST(CvVoltsToTranspose, AboveEightVoltsClampToEight) {
    // Chromatic scale = identity, so we can read the clamped value directly.
    EXPECT_EQ(cvVoltsToTranspose(8.0f,  Scale::Chromatic), 96);
    EXPECT_EQ(cvVoltsToTranspose(9.0f,  Scale::Chromatic), 96);
    EXPECT_EQ(cvVoltsToTranspose(20.0f, Scale::Chromatic), 96);
}

// ---------------------------------------------------------------------------
// Octave-boundary semitone snapping (no scale snap impact)
// ---------------------------------------------------------------------------

TEST(CvVoltsToTranspose, OneVoltIsOneOctave) {
    for (auto s : {Scale::Major, Scale::NaturalMinor, Scale::Chromatic}) {
        EXPECT_EQ(cvVoltsToTranspose(1.0f, s), 12) << "scale=" << (int)s;
    }
}

TEST(CvVoltsToTranspose, TwoVoltsIsTwoOctaves) {
    EXPECT_EQ(cvVoltsToTranspose(2.0f, Scale::Major),     24);
    EXPECT_EQ(cvVoltsToTranspose(2.0f, Scale::Chromatic), 24);
}

TEST(CvVoltsToTranspose, ChromaticIsSemitoneIdentity) {
    // Chromatic scale doesn't snap, so output should match raw rounded semitones.
    EXPECT_EQ(cvVoltsToTranspose(0.083f,  Scale::Chromatic), 1);  // ≈1/12
    EXPECT_EQ(cvVoltsToTranspose(0.166f,  Scale::Chromatic), 2);
    EXPECT_EQ(cvVoltsToTranspose(0.5f,    Scale::Chromatic), 6);
    EXPECT_EQ(cvVoltsToTranspose(0.917f,  Scale::Chromatic), 11); // ≈11/12
}

// ---------------------------------------------------------------------------
// Scale-snap behaviour
// ---------------------------------------------------------------------------

TEST(CvVoltsToTranspose, MajorSnapAtTritone) {
    // 6 semis (F♯) is not in Major. Adjacent: 5 (F, dist 1) and 7 (G, dist 1).
    // Tie breaks downward → 5.
    EXPECT_EQ(cvVoltsToTranspose(0.5f, Scale::Major), 5);
}

TEST(CvVoltsToTranspose, MajorSnapPreservesOctave) {
    // 1.5 V = 18 raw semis = 1 octave + 6 rem. Same snap as above → 12 + 5 = 17.
    EXPECT_EQ(cvVoltsToTranspose(1.5f, Scale::Major), 17);
}

TEST(CvVoltsToTranspose, PentMinSnapsCorrectly) {
    // Pent Min intervals: 0, 3, 5, 7, 10.
    //   1 semi → snap to 0 (dist 1) vs 3 (dist 2) → 0
    //   4 semi → snap to 3 (dist 1) vs 5 (dist 1) → 3 (tie down)
    //   6 semi → snap to 5 (dist 1) vs 7 (dist 1) → 5
    EXPECT_EQ(cvVoltsToTranspose(1.0f / 12.0f, Scale::PentatonicMinor), 0);
    EXPECT_EQ(cvVoltsToTranspose(4.0f / 12.0f, Scale::PentatonicMinor), 3);
    EXPECT_EQ(cvVoltsToTranspose(6.0f / 12.0f, Scale::PentatonicMinor), 5);
}

TEST(CvVoltsToTranspose, InScaleNotesPassThrough) {
    // Major intervals = {0,2,4,5,7,9,11}. 7/12 V = 7 semis (G), already in Major.
    EXPECT_EQ(cvVoltsToTranspose(7.0f / 12.0f, Scale::Major), 7);
    EXPECT_EQ(cvVoltsToTranspose(9.0f / 12.0f, Scale::Major), 9);
    EXPECT_EQ(cvVoltsToTranspose(11.0f / 12.0f, Scale::Major), 11);
}

// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
