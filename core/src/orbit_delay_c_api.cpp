#include "core/include/orbit_delay_c_api.h"

#include <new>

#include "core/include/orbit_delay_core.h"

struct OrbitDelayHandle {
    orbit::dsp::OrbitDelayCore core;
};

extern "C" {

OrbitDelayHandle* orbit_init(float sample_rate,
                             float* delay_buffer_l,
                             uint32_t delay_size_l,
                             float* delay_buffer_r,
                             uint32_t delay_size_r) {
    if (delay_buffer_l == nullptr || delay_size_l == 0u) {
        return nullptr;
    }

    OrbitDelayHandle* handle = new (std::nothrow) OrbitDelayHandle();
    if (handle == nullptr) {
        return nullptr;
    }

    if (!handle->core.attachBuffers(delay_buffer_l, delay_size_l, delay_buffer_r, delay_size_r)) {
        delete handle;
        return nullptr;
    }

    handle->core.reset(sample_rate);
    return handle;
}

void orbit_free(OrbitDelayHandle* handle) {
    delete handle;
}

void orbit_reset(OrbitDelayHandle* handle, float sample_rate) {
    if (handle == nullptr) {
        return;
    }

    handle->core.reset(sample_rate);
}

bool orbit_set_orbit(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setOrbit(value);
    return true;
}

bool orbit_set_offset_samples(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setOffsetSamples(value);
    return true;
}

bool orbit_set_stereo_spread(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setStereoSpread(value);
    return true;
}

bool orbit_set_feedback(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setFeedback(value);
    return true;
}

bool orbit_set_mix(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setMix(value);
    return true;
}

bool orbit_set_input_gain(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setInputGain(value);
    return true;
}

bool orbit_set_output_gain(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setOutputGain(value);
    return true;
}

bool orbit_set_lowpass_cutoff_hz(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setLowpassCutoffHz(value);
    return true;
}

bool orbit_set_diffusion(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setDiffusion(value);
    return true;
}

bool orbit_set_diffuser_stages(OrbitDelayHandle* handle, uint32_t value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setDiffuserStages(value);
    return true;
}

bool orbit_set_dc_block_enabled(OrbitDelayHandle* handle, bool enabled) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setDcBlockEnabled(enabled);
    return true;
}

bool orbit_process_stereo(OrbitDelayHandle* handle,
                          const float* input_l,
                          const float* input_r,
                          float* output_l,
                          float* output_r,
                          uint32_t num_samples) {
    if (handle == nullptr || input_l == nullptr || input_r == nullptr || output_l == nullptr || output_r == nullptr) {
        return false;
    }

    handle->core.processStereo(input_l, input_r, output_l, output_r, num_samples);
    return true;
}

} // extern "C"
