#include <gtest/gtest.h>
#include <cstdlib>
#include "arp.h"

static const uint8_t CHORD[] = {60, 64, 67, 72}; // C4, E4, G4, C5
static const uint8_t TWO[]   = {60, 67};         // for fallback tests

class ArpTest : public ::testing::Test {
protected:
    Arp arp;
};

// ---------------------------------------------------------------------------
// Up
// ---------------------------------------------------------------------------

TEST_F(ArpTest, UpCyclesNotesInOrder) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::Up);
    const uint8_t expected[] = {60, 64, 67, 72, 60, 64, 67, 72};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

// ---------------------------------------------------------------------------
// Down
// ---------------------------------------------------------------------------

TEST_F(ArpTest, DownCyclesReversed) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::Down);
    const uint8_t expected[] = {72, 67, 64, 60, 72, 67, 64, 60};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

// ---------------------------------------------------------------------------
// UpDownOpen — palindrome, endpoints not repeated (decisions.md §24)
// ---------------------------------------------------------------------------

TEST_F(ArpTest, UpDownOpenPalindromicNoRepeatEndpoints) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::UpDownOpen);
    // 0,1,2,3,2,1, then loops back to 0,1,2,3,2,1 (period 2*N-2 = 6)
    const uint8_t expected[] = {60, 64, 67, 72, 67, 64, 60, 64, 67, 72, 67, 64};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

TEST_F(ArpTest, UpDownOpenTwoNotesSafe) {
    arp.setNotes(TWO, 2);
    arp.setOrder(ArpOrder::UpDownOpen);
    // count=2 → period=2 → indices 0,1,0,1...
    const uint8_t expected[] = {60, 67, 60, 67};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

// ---------------------------------------------------------------------------
// UpDownClosed — palindrome, endpoints repeated (decisions.md §24)
// ---------------------------------------------------------------------------

TEST_F(ArpTest, UpDownClosedPalindromicWithRepeatEndpoints) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::UpDownClosed);
    // 0,1,2,3,3,2,1,0, then loops back to 0,1,2,3,3,2,1,0 (period 2*N = 8)
    const uint8_t expected[] = {60, 64, 67, 72, 72, 67, 64, 60, 60, 64, 67, 72, 72, 67, 64, 60};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

TEST_F(ArpTest, UpDownClosedTwoNotesSafe) {
    arp.setNotes(TWO, 2);
    arp.setOrder(ArpOrder::UpDownClosed);
    // count=2 → period=4 → indices 0,1,1,0,0,1,1,0...
    const uint8_t expected[] = {60, 67, 67, 60, 60, 67, 67, 60};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

TEST_F(ArpTest, UpDownClosedRepeatsTopAndBottomAcrossLoop) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::UpDownClosed);
    // Step through two full cycles, look for the 0→0 boundary repeat.
    uint8_t bottomCount = 0;
    uint8_t topCount    = 0;
    for (int i = 0; i < 16; ++i) {
        uint8_t n = arp.nextNote();
        if (n == 60) bottomCount++;
        if (n == 72) topCount++;
    }
    // 16 steps = 2 full cycles. Each endpoint is doubled once per cycle → 4 hits each.
    EXPECT_EQ(bottomCount, 4);
    EXPECT_EQ(topCount,    4);
}

// ---------------------------------------------------------------------------
// SkipUp (1-3-2-4 permutation)
// ---------------------------------------------------------------------------

TEST_F(ArpTest, SkipUpPicksIndices0213) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::SkipUp);
    // pattern indices: 0,2,1,3 → notes 60,67,64,72
    const uint8_t expected[] = {60, 67, 64, 72, 60, 67, 64, 72};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

TEST_F(ArpTest, SkipUpFallsBackToUpForFewerThan4Notes) {
    arp.setNotes(TWO, 2);
    arp.setOrder(ArpOrder::SkipUp);
    const uint8_t expected[] = {60, 67, 60, 67};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

// ---------------------------------------------------------------------------
// Random — distribution + determinism via srand seeding
// ---------------------------------------------------------------------------

TEST_F(ArpTest, RandomReturnsAllValidIndicesOverManyCalls) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::Random);
    std::srand(42);
    uint8_t seen[4] = {0, 0, 0, 0};
    for (int i = 0; i < 200; ++i) {
        uint8_t n = arp.nextNote();
        // Map back to chord index by linear search (safe for size 4).
        for (uint8_t j = 0; j < 4; ++j) {
            if (CHORD[j] == n) { seen[j]++; break; }
        }
    }
    // All four notes must have been hit at least once.
    for (uint8_t j = 0; j < 4; ++j) EXPECT_GT(seen[j], 0u) << "note " << (int)CHORD[j] << " never selected";
}

TEST_F(ArpTest, RandomDistributionIsRoughlyUniform) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::Random);
    std::srand(123);
    int counts[4] = {0, 0, 0, 0};
    const int N = 4000;
    for (int i = 0; i < N; ++i) {
        uint8_t n = arp.nextNote();
        for (uint8_t j = 0; j < 4; ++j) {
            if (CHORD[j] == n) { counts[j]++; break; }
        }
    }
    // Expected ~1000 per bin; allow ±20% slack to keep the test stable.
    for (int j = 0; j < 4; ++j) {
        EXPECT_GE(counts[j], (int)(N / 4 * 0.8));
        EXPECT_LE(counts[j], (int)(N / 4 * 1.2));
    }
}

TEST_F(ArpTest, RandomDeterministicWhenSeeded) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::Random);
    std::srand(7);
    uint8_t a[16];
    for (int i = 0; i < 16; ++i) a[i] = arp.nextNote();
    arp.reset();
    std::srand(7);
    for (int i = 0; i < 16; ++i) EXPECT_EQ(arp.nextNote(), a[i]);
}

// ---------------------------------------------------------------------------
// State management
// ---------------------------------------------------------------------------

TEST_F(ArpTest, ResetReturnsToFirstNote) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::Up);
    (void)arp.nextNote(); // 60
    (void)arp.nextNote(); // 64
    arp.reset();
    EXPECT_EQ(arp.nextNote(), 60);
}

TEST_F(ArpTest, SetOrderResetsStep) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::Up);
    (void)arp.nextNote(); // 60 → step now 1
    arp.setOrder(ArpOrder::Down);
    EXPECT_EQ(arp.nextNote(), 72); // step reset, Down starts at top
}

TEST_F(ArpTest, NoNotesReturnsZeroSafely) {
    EXPECT_EQ(arp.nextNote(), 0);
}

// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
