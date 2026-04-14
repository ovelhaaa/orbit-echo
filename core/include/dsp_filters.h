#pragma once

#include <cmath>

#include "core/include/dsp_utils.h"

namespace orbit::dsp {

struct BiquadLowpass {
    float sampleRate = 48000.0f;
    float cutoffHz = 8000.0f;
    float q = 0.707f;
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;

    void reset(float state = 0.0f) {
        z1 = state;
        z2 = state;
    }

    void setSampleRate(float sr) {
        sampleRate = clampf(sr, 1.0f, 384000.0f);
        updateCoefficients();
    }

    void setCutoffHz(float hz) {
        setParams(hz, q);
    }

    void setQ(float value) {
        setParams(cutoffHz, value);
    }

    void setParams(float hz, float qValue) {
        cutoffHz = clampf(hz, 1.0f, 0.49f * sampleRate);
        q = clampf(qValue, 0.1f, 10.0f);
        updateCoefficients();
    }

    float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    void updateCoefficients() {
        const float w0 = 2.0f * kPi * cutoffHz / sampleRate;
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 / (2.0f * q);
        const float a0 = 1.0f + alpha;

        const float b0Raw = (1.0f - cosW0) * 0.5f;
        const float b1Raw = 1.0f - cosW0;
        const float b2Raw = b0Raw;
        const float a1Raw = -2.0f * cosW0;
        const float a2Raw = 1.0f - alpha;

        b0 = b0Raw / a0;
        b1 = b1Raw / a0;
        b2 = b2Raw / a0;
        a1 = a1Raw / a0;
        a2 = a2Raw / a0;
    }
};

using OnePoleLowpass [[deprecated("Use BiquadLowpass")]] = BiquadLowpass;

struct DCBlocker {
    float sampleRate = 48000.0f;
    float cutoffHz = 20.0f;
    float r = 0.995f;
    float x1 = 0.0f;
    float y1 = 0.0f;

    void reset() {
        x1 = 0.0f;
        y1 = 0.0f;
    }

    void setSampleRate(float sr) {
        sampleRate = clampf(sr, 1.0f, 384000.0f);
        updateR();
    }

    void setCutoffHz(float hz) {
        cutoffHz = clampf(hz, 1.0f, 200.0f);
        updateR();
    }

    float process(float x) {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    void updateR() {
        const float pole = std::exp(-2.0f * kPi * cutoffHz / sampleRate);
        r = clampf(pole, 0.0f, 0.99999f);
    }
};

} // namespace orbit::dsp