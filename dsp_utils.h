#pragma once

#include <cstdint>
#include <cmath>

namespace orbit::dsp {

inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

inline bool isFiniteSafe(float x) {
    return std::isfinite(x);
}

inline int32_t wrapIndexInt(int32_t idx, uint32_t size) {
    const int32_t s = static_cast<int32_t>(size);
    while (idx < 0) {
        idx += s;
    }
    while (idx >= s) {
        idx -= s;
    }
    return idx;
}

inline float wrapPosFloat(float pos, float size) {
    while (pos < 0.0f) {
        pos += size;
    }
    while (pos >= size) {
        pos -= size;
    }
    return pos;
}

inline uint32_t wrapIncrement(uint32_t idx, uint32_t size) {
    ++idx;
    return (idx >= size) ? 0u : idx;
}

} // namespace orbit::dsp
