#include "inputs.h"

#include <Arduino.h>
#include <MCP_ADC.h>

namespace inputs {

namespace {
MCP3208* g_adc = nullptr;

float g_vref = 4.096f;

struct Calibration {
    float gain   = 1.0f;
    float offset = 0.0f;
};
Calibration g_cal[CHANNEL_COUNT];

float countToVolts(uint16_t count) {
    if (count > 4095) count = 4095;
    return (static_cast<float>(count) / 4095.0f) * g_vref;
}
}  // namespace

bool begin(uint8_t cs_pin) {
    static MCP3208 adc;
    g_adc = &adc;
    g_adc->begin(cs_pin);
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

float readVolts(uint8_t channel) {
    if (channel >= CHANNEL_COUNT || g_adc == nullptr) return 0.0f;
    int raw = g_adc->read(channel);
    if (raw < 0) raw = 0;
    float adc_volts = countToVolts(static_cast<uint16_t>(raw));
    const Calibration& c = g_cal[channel];
    return c.gain * adc_volts + c.offset;
}

uint16_t readRaw(uint8_t channel) {
    if (channel >= CHANNEL_COUNT || g_adc == nullptr) return 0;
    int raw = g_adc->read(channel);
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return static_cast<uint16_t>(raw);
}

}  // namespace inputs
