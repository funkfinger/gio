#include <gtest/gtest.h>
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
// UpDown — palindrome, endpoints not repeated
// ---------------------------------------------------------------------------

TEST_F(ArpTest, UpDownPalindromicNoRepeatEndpoints) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::UpDown);
    // 0,1,2,3,2,1, then loops back to 0,1,2,3,2,1
    const uint8_t expected[] = {60, 64, 67, 72, 67, 64, 60, 64, 67, 72, 67, 64};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

TEST_F(ArpTest, UpDownTwoNotesSafe) {
    arp.setNotes(TWO, 2);
    arp.setOrder(ArpOrder::UpDown);
    // count=2 → period=2 → indices 0,1,0,1...
    const uint8_t expected[] = {60, 67, 60, 67};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

// ---------------------------------------------------------------------------
// Order1324
// ---------------------------------------------------------------------------

TEST_F(ArpTest, Order1324PicksIndices0213) {
    arp.setNotes(CHORD, 4);
    arp.setOrder(ArpOrder::Order1324);
    // pattern indices: 0,2,1,3 → notes 60,67,64,72
    const uint8_t expected[] = {60, 67, 64, 72, 60, 67, 64, 72};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
}

TEST_F(ArpTest, Order1324FallsBackToUpForFewerThan4Notes) {
    arp.setNotes(TWO, 2);
    arp.setOrder(ArpOrder::Order1324);
    const uint8_t expected[] = {60, 67, 60, 67};
    for (uint8_t e : expected) EXPECT_EQ(arp.nextNote(), e);
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
