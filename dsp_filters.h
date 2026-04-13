#pragma once

#include <cmath>

#include "dsp_utils.h"

namespace orbit::dsp {

struct OnePoleLowpass {
    float sampleRate = 48000.0f;
    float cutoffHz = 8000.0f;
    float alpha = 1.0f;
    float z = 0.0f;

    void reset(float state = 0.0f) {
        z = state;
    }

    void setSampleRate(float sr) {
        sampleRate = clampf(sr, 1.0f, 384000.0f);
        updateAlpha();
    }

    void setCutoffHz(float hz) {
        cutoffHz = clampf(hz, 1.0f, 0.49f * sampleRate);
        updateAlpha();
    }

    float process(float x) {
        z += alpha * (x - z);
        return z;
    }

private:
    void updateAlpha() {
        const float wc = 2.0f * kPi * cutoffHz;
        alpha = wc / (wc + sampleRate);
        alpha = clampf(alpha, 0.0f, 1.0f);
    }
};

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
