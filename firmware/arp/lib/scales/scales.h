#pragma once
#include <cstdint>

enum class Scale : uint8_t {
    Major = 0,
    NaturalMinor,
    PentatonicMajor,
    PentatonicMinor,
    Dorian,
    Chromatic,
    COUNT
};

// Returns the nearest in-scale MIDI note. Ties (equidistant) break downward.
uint8_t quantize(uint8_t midiNote, Scale scale);

// Maps a normalised pot reading [0.0, 1.0] to a Scale with ±2 % hysteresis.
Scale scaleFromPot(float pot, Scale current);
