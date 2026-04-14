#include "core/include/orbit_delay_c_api.h"

#include <new>

#include "core/include/orbit_delay_core.h"

struct OrbitDelayHandle {
    orbit::dsp::OrbitDelayCore core;
};

namespace {

OrbitDelayHandle* createHandle() {
    return new (std::nothrow) OrbitDelayHandle();
}

} // namespace

extern "C" {

OrbitDelayHandle* orbit_init(float sample_rate,
                             float* delay_buffer_l,
                             uint32_t delay_size_l,
                             float* delay_buffer_r,
                             uint32_t delay_size_r) {
    if (delay_buffer_l == nullptr || delay_buffer_r == nullptr || delay_size_l == 0u || delay_size_r == 0u) {
        return nullptr;
    }

    OrbitDelayHandle* handle = createHandle();
    if (handle == nullptr) {
        return nullptr;
    }

    if (delay_size_l != delay_size_r) {
        delete handle;
        return nullptr;
    }

    if (!handle->core.attachBuffers(delay_buffer_l, delay_buffer_r, delay_size_l)) {
        delete handle;
        return nullptr;
    }

    handle->core.reset(sample_rate);
    return handle;
}

OrbitDelayHandle* orbit_init_mono(float sample_rate, float* delay_buffer, uint32_t delay_size) {
    if (delay_buffer == nullptr || delay_size == 0u) {
        return nullptr;
    }

    OrbitDelayHandle* handle = createHandle();
    if (handle == nullptr) {
        return nullptr;
    }

    if (!handle->core.attachBufferMono(delay_buffer, delay_size)) {
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

bool orbit_set_tone_hz(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setToneHz(value);
    return true;
}

bool orbit_set_smear_amount(OrbitDelayHandle* handle, float value) {
    if (handle == nullptr) {
        return false;
    }
    handle->core.setSmearAmount(value);
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

bool orbit_set_read_mode(OrbitDelayHandle* handle, OrbitReadMode mode) {
    if (handle == nullptr) {
        return false;
    }

    switch (mode) {
        case ORBIT_READ_MODE_ORBIT:
            handle->core.setReadMode(orbit::dsp::OrbitDelayCore::ReadMode::Orbit);
            return true;
        case ORBIT_READ_MODE_ACCIDENTAL_REVERSE:
            handle->core.setReadMode(orbit::dsp::OrbitDelayCore::ReadMode::AccidentalReverse);
            return true;
        default:
            return false;
    }
}

bool orbit_attach_buffers(OrbitDelayHandle* handle, float* delay_buffer_l, float* delay_buffer_r, uint32_t delay_size) {
    if (handle == nullptr || delay_buffer_l == nullptr || delay_buffer_r == nullptr) {
        return false;
    }
    return handle->core.attachBuffers(delay_buffer_l, delay_buffer_r, delay_size);
}

bool orbit_attach_buffer_mono(OrbitDelayHandle* handle, float* delay_buffer, uint32_t delay_size) {
    if (handle == nullptr || delay_buffer == nullptr) {
        return false;
    }
    return handle->core.attachBufferMono(delay_buffer, delay_size);
}

bool orbit_set_lowpass_cutoff_hz(OrbitDelayHandle* handle, float value) {
    return orbit_set_tone_hz(handle, value);
}

bool orbit_set_diffusion(OrbitDelayHandle* handle, float value) {
    return orbit_set_smear_amount(handle, value);
}

bool orbit_process_mono(OrbitDelayHandle* handle,
                        const float* input,
                        float* output,
                        uint32_t num_samples) {
    if (handle == nullptr || input == nullptr || output == nullptr) {
        return false;
    }

    handle->core.processMono(input, output, num_samples);
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
