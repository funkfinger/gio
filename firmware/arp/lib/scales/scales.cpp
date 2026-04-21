#include "scales.h"

static const uint8_t NUM_SCALES = static_cast<uint8_t>(Scale::COUNT);

// Semitone intervals from root for each scale.
static const uint8_t MAJOR[]      = {0, 2, 4, 5, 7, 9, 11};
static const uint8_t NAT_MINOR[]  = {0, 2, 3, 5, 7, 8, 10};
static const uint8_t PENT_MAJ[]   = {0, 2, 4, 7, 9};
static const uint8_t PENT_MIN[]   = {0, 3, 5, 7, 10};
static const uint8_t DORIAN[]     = {0, 2, 3, 5, 7, 9, 10};
static const uint8_t CHROMATIC[]  = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static const uint8_t* const INTERVALS[NUM_SCALES] = {
    MAJOR, NAT_MINOR, PENT_MAJ, PENT_MIN, DORIAN, CHROMATIC
};
static const uint8_t COUNTS[NUM_SCALES] = {7, 7, 5, 5, 7, 12};

// Returns a 12-bit bitmask: bit i set when semitone i is in the scale.
static uint16_t scaleMask(Scale scale) {
    uint8_t idx = static_cast<uint8_t>(scale);
    uint16_t mask = 0;
    for (uint8_t i = 0; i < COUNTS[idx]; ++i)
        mask |= static_cast<uint16_t>(1u << INTERVALS[idx][i]);
    return mask;
}

uint8_t quantize(uint8_t midiNote, Scale scale) {
    if (scale == Scale::Chromatic) return midiNote;

    uint8_t semitone = midiNote % 12;
    uint16_t mask = scaleMask(scale);

    int bestDist = 13;
    int bestOffset = 0;

    // Sweep offsets -6..+6; ties prefer negative (downward).
    for (int offset = -6; offset <= 6; ++offset) {
        int s = ((int)semitone + offset + 12) % 12;
        if (!(mask & (1u << s))) continue;
        int dist = offset < 0 ? -offset : offset;
        if (dist < bestDist || (dist == bestDist && offset < bestOffset)) {
            bestDist = dist;
            bestOffset = offset;
        }
    }

    int result = (int)midiNote + bestOffset;
    if (result < 0)   result = 0;
    if (result > 127) result = 127;
    return static_cast<uint8_t>(result);
}

Scale scaleFromPot(float pot, Scale current) {
    const float zoneWidth = 1.0f / (float)NUM_SCALES;
    const float hyst = 0.02f;
    uint8_t cur = static_cast<uint8_t>(current);
    float lower = cur * zoneWidth;
    float upper = (cur + 1) * zoneWidth;

    if (pot >= upper + hyst || pot < lower - hyst) {
        int next = (int)(pot / zoneWidth);
        if (next < 0) next = 0;
        if (next >= NUM_SCALES) next = NUM_SCALES - 1;
        return static_cast<Scale>(next);
    }
    return current;
}
