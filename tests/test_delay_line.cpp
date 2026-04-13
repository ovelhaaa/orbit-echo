#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "core/include/dsp_delay_line.h"

namespace {

bool nearlyEqual(float a, float b, float eps = 1.0e-6f) {
    return std::fabs(a - b) <= eps;
}

int fail(const char* message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    using orbit::dsp::DelayLine;

    std::vector<float> storage(4u, 0.0f);
    DelayLine delay;
    if (!delay.attach(storage.data(), static_cast<uint32_t>(storage.size()))) {
        return fail("attach should succeed with valid buffer");
    }

    // Wrap circular de escrita.
    delay.write(1.0f);
    delay.write(2.0f);
    delay.write(3.0f);
    delay.write(4.0f);
    if (delay.writePos != 0u) {
        return fail("writePos should wrap back to zero");
    }

    // Nova escrita deve sobrepor início do buffer circular.
    delay.write(5.0f);
    if (!nearlyEqual(storage[0], 5.0f)) {
        return fail("first slot should be overwritten after wrap");
    }

    // Leitura fracionária + interpolação linear entre índices 0 e 1.
    const float sample01 = delay.readAbsoluteLinear(0.5f);
    const float expected01 = storage[0] + 0.5f * (storage[1] - storage[0]);
    if (!nearlyEqual(sample01, expected01)) {
        return fail("linear interpolation at 0.5 should match midpoint");
    }

    // Interpolação linear com wrap entre último e primeiro elemento.
    const float sampleWrap = delay.readAbsoluteLinear(3.5f);
    const float expectedWrap = storage[3] + 0.5f * (storage[0] - storage[3]);
    if (!nearlyEqual(sampleWrap, expectedWrap)) {
        return fail("linear interpolation across wrap boundary should match");
    }

    // Leitura com posição negativa deve fazer wrap.
    const float sampleNegative = delay.readAbsoluteLinear(-0.5f);
    if (!nearlyEqual(sampleNegative, expectedWrap)) {
        return fail("negative positions should wrap to the same value as 3.5");
    }

    std::cout << "test_delay_line: OK\n";
    return 0;
}
