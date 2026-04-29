#include "outputs.h"

#include <Arduino.h>
#include <DAC8552.h>

namespace outputs {

namespace {
DAC8552* g_dac = nullptr;

float g_vref = 4.096f;

struct Calibration {
    float gain   = 1.0f;
    float offset = 0.0f;
};
Calibration g_cal[CHANNEL_COUNT];

uint16_t voltsToCount(float volts) {
    if (volts < 0.0f) volts = 0.0f;
    if (volts > g_vref) volts = g_vref;
    float fcount = (volts / g_vref) * 65535.0f;
    if (fcount < 0.0f) fcount = 0.0f;
    if (fcount > 65535.0f) fcount = 65535.0f;
    return static_cast<uint16_t>(fcount + 0.5f);
}
}  // namespace

bool begin(uint8_t cs_pin) {
    static DAC8552 dac(cs_pin);
    g_dac = &dac;
    g_dac->begin();
    // Park both channels at 0 V so any downstream stage (op-amp, jack) starts
    // in a known state instead of whatever the DAC's POR happens to leave.
    g_dac->setValue(0, 0);
    g_dac->setValue(1, 0);
    return true;
}

void setVRef(float vref_volts) {
    if (vref_volts > 0.0f) g_vref = vref_volts;
}

float getVRef() {
    return g_vref;
}

void setCalibration(uint8_t channel, float gain, float offset) {
    if (channel >= CHANNEL_COUNT) return;
    g_cal[channel].gain   = gain;
    g_cal[channel].offset = offset;
}

uint16_t write(uint8_t channel, float volts) {
    if (channel >= CHANNEL_COUNT || g_dac == nullptr) return 0;
    const Calibration& c = g_cal[channel];
    float dac_volts = c.gain * volts + c.offset;
    uint16_t count = voltsToCount(dac_volts);
    g_dac->setValue(channel, count);
    return count;
}

void gate(uint8_t channel, bool on) {
    write(channel, on ? g_vref : 0.0f);
}

void writeRaw(uint8_t channel, uint16_t count) {
    if (channel >= CHANNEL_COUNT || g_dac == nullptr) return;
    g_dac->setValue(channel, count);
}

}  // namespace outputs
