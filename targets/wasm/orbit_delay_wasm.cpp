#include "core/include/orbit_delay_c_api.h"

#include <cstdint>
#include <new>

namespace {

struct WasmOrbitContext {
    OrbitDelayHandle* handle = nullptr;
    float* delay_l = nullptr;
    float* delay_r = nullptr;
    uint32_t delay_size = 0;
};

WasmOrbitContext g_ctx;

void clear_context() {
    g_ctx.handle = nullptr;
    g_ctx.delay_l = nullptr;
    g_ctx.delay_r = nullptr;
    g_ctx.delay_size = 0;
}

} // namespace

extern "C" {

bool orbit_wasm_init(float sample_rate, uint32_t delay_size) {
    if (delay_size == 0u) {
        return false;
    }

    orbit_free(g_ctx.handle);
    delete[] g_ctx.delay_l;
    delete[] g_ctx.delay_r;
    clear_context();

    g_ctx.delay_l = new (std::nothrow) float[delay_size]();
    g_ctx.delay_r = new (std::nothrow) float[delay_size]();
    if (g_ctx.delay_l == nullptr || g_ctx.delay_r == nullptr) {
        delete[] g_ctx.delay_l;
        delete[] g_ctx.delay_r;
        clear_context();
        return false;
    }

    g_ctx.handle = orbit_init(sample_rate, g_ctx.delay_l, delay_size, g_ctx.delay_r, delay_size);
    if (g_ctx.handle == nullptr) {
        delete[] g_ctx.delay_l;
        delete[] g_ctx.delay_r;
        clear_context();
        return false;
    }

    g_ctx.delay_size = delay_size;
    return true;
}

void orbit_wasm_free() {
    orbit_free(g_ctx.handle);
    delete[] g_ctx.delay_l;
    delete[] g_ctx.delay_r;
    clear_context();
}

bool orbit_wasm_reset(float sample_rate) {
    if (g_ctx.handle == nullptr) {
        return false;
    }
    orbit_reset(g_ctx.handle, sample_rate);
    return true;
}

bool orbit_wasm_process_stereo(const float* input_l,
                               const float* input_r,
                               float* output_l,
                               float* output_r,
                               uint32_t num_samples) {
    if (g_ctx.handle == nullptr) {
        return false;
    }

    return orbit_process_stereo(g_ctx.handle, input_l, input_r, output_l, output_r, num_samples);
}

bool orbit_wasm_set_orbit(float value) { return orbit_set_orbit(g_ctx.handle, value); }
bool orbit_wasm_set_offset_samples(float value) { return orbit_set_offset_samples(g_ctx.handle, value); }
bool orbit_wasm_set_tempo_bpm(float value) { return orbit_set_tempo_bpm(g_ctx.handle, value); }
bool orbit_wasm_set_note_division(float value) { return orbit_set_note_division(g_ctx.handle, value); }
bool orbit_wasm_set_stereo_spread(float value) { return orbit_set_stereo_spread(g_ctx.handle, value); }
bool orbit_wasm_set_feedback(float value) { return orbit_set_feedback(g_ctx.handle, value); }
bool orbit_wasm_set_mix(float value) { return orbit_set_mix(g_ctx.handle, value); }
bool orbit_wasm_set_input_gain(float value) { return orbit_set_input_gain(g_ctx.handle, value); }
bool orbit_wasm_set_output_gain(float value) { return orbit_set_output_gain(g_ctx.handle, value); }
bool orbit_wasm_set_tone_hz(float value) { return orbit_set_tone_hz(g_ctx.handle, value); }
bool orbit_wasm_set_smear_amount(float value) { return orbit_set_smear_amount(g_ctx.handle, value); }
bool orbit_wasm_set_diffuser_stages(uint32_t value) { return orbit_set_diffuser_stages(g_ctx.handle, value); }
bool orbit_wasm_set_dc_block_enabled(bool enabled) { return orbit_set_dc_block_enabled(g_ctx.handle, enabled); }
bool orbit_wasm_set_read_mode(uint32_t mode) { return orbit_set_read_mode(g_ctx.handle, static_cast<OrbitReadMode>(mode)); }

} // extern "C"
