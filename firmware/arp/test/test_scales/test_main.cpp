#include <gtest/gtest.h>
#include "scales.h"

// ---------------------------------------------------------------------------
// quantize — in-scale notes pass through unchanged
// ---------------------------------------------------------------------------

TEST(Quantize, MajorInScalePassThrough) {
    // C Major semitones: 0,2,4,5,7,9,11
    const uint8_t inScale[] = {0, 2, 4, 5, 7, 9, 11};
    for (uint8_t s : inScale) {
        uint8_t note = 60 + s; // middle-C octave
        EXPECT_EQ(quantize(note, Scale::Major), note) << "semitone " << (int)s;
    }
}

TEST(Quantize, NatMinorInScalePassThrough) {
    const uint8_t inScale[] = {0, 2, 3, 5, 7, 8, 10};
    for (uint8_t s : inScale)
        EXPECT_EQ(quantize(60 + s, Scale::NaturalMinor), 60 + s);
}

TEST(Quantize, PentMajorInScalePassThrough) {
    const uint8_t inScale[] = {0, 2, 4, 7, 9};
    for (uint8_t s : inScale)
        EXPECT_EQ(quantize(60 + s, Scale::PentatonicMajor), 60 + s);
}

TEST(Quantize, PentMinorInScalePassThrough) {
    const uint8_t inScale[] = {0, 3, 5, 7, 10};
    for (uint8_t s : inScale)
        EXPECT_EQ(quantize(60 + s, Scale::PentatonicMinor), 60 + s);
}

TEST(Quantize, DorianInScalePassThrough) {
    const uint8_t inScale[] = {0, 2, 3, 5, 7, 9, 10};
    for (uint8_t s : inScale)
        EXPECT_EQ(quantize(60 + s, Scale::Dorian), 60 + s);
}

TEST(Quantize, ChromaticIdentity) {
    for (uint8_t n = 0; n <= 127; ++n)
        EXPECT_EQ(quantize(n, Scale::Chromatic), n);
}

// ---------------------------------------------------------------------------
// quantize — out-of-scale notes snap to nearest; ties break downward
// ---------------------------------------------------------------------------

TEST(Quantize, MajorOutOfScale) {
    // C# (61) → C (60) or D (62), both dist 1; tie breaks downward → C
    EXPECT_EQ(quantize(61, Scale::Major), 60);
    // D# (63) → D (62) or E (64), tie → D
    EXPECT_EQ(quantize(63, Scale::Major), 62);
    // F# (66) → F (65) or G (67), tie → F
    EXPECT_EQ(quantize(66, Scale::Major), 65);
    // A# (70) → A (69) or B (71), tie → A
    EXPECT_EQ(quantize(70, Scale::Major), 69);
}

TEST(Quantize, PentMajorOutOfScale) {
    // C# (61) → C (60) dist 1 vs D (62) dist 1; tie → C
    EXPECT_EQ(quantize(61, Scale::PentatonicMajor), 60);
    // D# (63) → D (62) dist 1 vs E (64) dist 1; tie → D
    EXPECT_EQ(quantize(63, Scale::PentatonicMajor), 62);
    // F (65) → E (64) dist 1 vs G (67) dist 2; → E
    EXPECT_EQ(quantize(65, Scale::PentatonicMajor), 64);
    // F# (66) → E (64) dist 2 vs G (67) dist 1; → G
    EXPECT_EQ(quantize(66, Scale::PentatonicMajor), 67);
}

// ---------------------------------------------------------------------------
// quantize — octave invariance
// ---------------------------------------------------------------------------

TEST(Quantize, OctaveInvariance) {
    for (uint8_t n = 0; n <= 115; ++n) { // +12 stays <= 127
        for (auto s : {Scale::Major, Scale::NaturalMinor, Scale::PentatonicMajor,
                       Scale::PentatonicMinor, Scale::Dorian, Scale::Chromatic}) {
            EXPECT_EQ((int)quantize(n + 12, s), (int)quantize(n, s) + 12)
                << "note=" << (int)n << " scale=" << (int)s;
        }
    }
}

// ---------------------------------------------------------------------------
// scaleFromPot — zone centres select expected scale
// ---------------------------------------------------------------------------

TEST(ScaleFromPot, ZoneCentres) {
    // Each zone centre should return the correct scale (no hysteresis conflict).
    const float w = 1.0f / 6.0f;
    Scale prev = Scale::Major;
    EXPECT_EQ(scaleFromPot(0.5f * w,        prev), Scale::Major);
    prev = Scale::Major;
    EXPECT_EQ(scaleFromPot(1.5f * w, Scale::NaturalMinor),  Scale::NaturalMinor);
    EXPECT_EQ(scaleFromPot(2.5f * w, Scale::PentatonicMajor), Scale::PentatonicMajor);
    EXPECT_EQ(scaleFromPot(3.5f * w, Scale::PentatonicMinor), Scale::PentatonicMinor);
    EXPECT_EQ(scaleFromPot(4.5f * w, Scale::Dorian),        Scale::Dorian);
    EXPECT_EQ(scaleFromPot(5.5f * w, Scale::Chromatic),     Scale::Chromatic);
}

// ---------------------------------------------------------------------------
// scaleFromPot — hysteresis: boundary alone does not switch
// ---------------------------------------------------------------------------

TEST(ScaleFromPot, HysteresisHoldsBelowThreshold) {
    const float boundary = 1.0f / 6.0f; // boundary between Major and NatMinor
    // Exactly at boundary: still in Major (within hysteresis)
    EXPECT_EQ(scaleFromPot(boundary, Scale::Major), Scale::Major);
    // 1 % past boundary: still Major (< 2 % hyst)
    EXPECT_EQ(scaleFromPot(boundary + 0.01f, Scale::Major), Scale::Major);
}

TEST(ScaleFromPot, HysteresisSwitchesAboveThreshold) {
    const float boundary = 1.0f / 6.0f;
    // 2.1 % past boundary: switches to NatMinor
    EXPECT_EQ(scaleFromPot(boundary + 0.021f, Scale::Major), Scale::NaturalMinor);
}

// ---------------------------------------------------------------------------
// scaleFromPot — full upward sweep visits all 6 scales in order
// ---------------------------------------------------------------------------

TEST(ScaleFromPot, SweepVisitsAllSixInOrder) {
    Scale cur = Scale::Major;
    bool visited[6] = {};
    visited[0] = true; // start in Major

    for (int i = 0; i <= 1000; ++i) {
        float pot = i / 1000.0f;
        cur = scaleFromPot(pot, cur);
        uint8_t idx = static_cast<uint8_t>(cur);
        if (idx < 6) visited[idx] = true;
    }

    for (int i = 0; i < 6; ++i)
        EXPECT_TRUE(visited[i]) << "Scale " << i << " never reached";
}

// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
