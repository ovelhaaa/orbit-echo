#pragma once

#include <cstdint>
#include <cstring>

#include "dsp_utils.h"

namespace orbit::dsp {

struct DelayLine {
    float* buffer = nullptr;
    uint32_t size = 0;
    uint32_t writePos = 0;

    bool attach(float* inBuffer, uint32_t sizeSamples) {
        if (inBuffer == nullptr || sizeSamples < 2u) {
            buffer = nullptr;
            size = 0;
            writePos = 0;
            return false;
        }
        buffer = inBuffer;
        size = sizeSamples;
        writePos = 0;
        clear();
        return true;
    }

    void clear() {
        if (buffer == nullptr || size == 0u) {
            return;
        }
        std::memset(buffer, 0, sizeof(float) * size);
        writePos = 0;
    }

    void write(float x) {
        if (buffer == nullptr || size == 0u) {
            return;
        }
        buffer[writePos] = x;
        writePos = wrapIncrement(writePos, size);
    }

    float readAbsoluteLinear(float pos) const {
        if (buffer == nullptr || size < 2u) {
            return 0.0f;
        }

        const float wrapped = wrapPosFloat(pos, static_cast<float>(size));
        const int32_t i0 = static_cast<int32_t>(wrapped);
        const int32_t i1 = (i0 + 1 >= static_cast<int32_t>(size)) ? 0 : (i0 + 1);
        const float frac = wrapped - static_cast<float>(i0);

        const float y0 = buffer[i0];
        const float y1 = buffer[i1];
        return y0 + (y1 - y0) * frac;
    }

#ifdef ORBIT_DELAY_ENABLE_HERMITE
    float readAbsoluteHermite(float pos) const {
        if (buffer == nullptr || size < 4u) {
            return readAbsoluteLinear(pos);
        }

        const float wrapped = wrapPosFloat(pos, static_cast<float>(size));
        const int32_t i1 = static_cast<int32_t>(wrapped);
        const int32_t i0 = (i1 == 0) ? static_cast<int32_t>(size - 1) : (i1 - 1);
        const int32_t i2 = (i1 + 1 >= static_cast<int32_t>(size)) ? 0 : (i1 + 1);
        const int32_t i3 = (i2 + 1 >= static_cast<int32_t>(size)) ? 0 : (i2 + 1);

        const float frac = wrapped - static_cast<float>(i1);
        const float xm1 = buffer[i0];
        const float x0 = buffer[i1];
        const float x1 = buffer[i2];
        const float x2 = buffer[i3];

        const float c = 0.5f * (x1 - xm1);
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + 0.5f * (x2 - x0);
        const float bNeg = w + a;
        return (((a * frac) - bNeg) * frac + c) * frac + x0;
    }
#endif
};

} // namespace orbit::dsp
