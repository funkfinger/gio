#include "cv.h"

int cvVoltsToTranspose(float volts, Scale scale) {
    if (volts < 0.0f) volts = 0.0f;
    if (volts > 8.0f) volts = 8.0f;

    int raw_semis = (int)(volts * 12.0f + 0.5f);   // round to nearest semitone

    // Scale-snap: keep octaves, snap pitch class within the octave.
    int octaves = raw_semis / 12;
    int rem     = raw_semis % 12;
    uint8_t snapped = quantize((uint8_t)rem, scale);  // returns 0..11 for rem in 0..11

    return octaves * 12 + (int)snapped;
}
