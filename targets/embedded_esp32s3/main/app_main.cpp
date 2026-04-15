#include "audio_engine_esp32.h"
#include "audio_source.h"
#include "board_config.h"
#include "parameter_bridge.h"
#include "ui_tft.h"

#include "core/include/orbit_delay_core.h"

#include <algorithm>
#include <cstdint>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace orbit::embedded {
namespace {
constexpr const char* kTag = "orbit_app";
// 0.5s @ 48kHz: mantém buffers críticos em SRAM interna com margem de heap no ESP32-S3.
constexpr uint32_t kMaxDelaySamples = 24000;

struct AppContext {
    dsp::OrbitDelayCore core;
    ParameterBridge params;
    AudioEngineEsp32 audio;
    UiTft ui;

    AudioSourceType sourceType = AudioSourceType::ExternalI2s;
    I2sInputSource externalI2sSource;
    InternalTestSilenceSource internalTestSource;

    float* delayBufferL = nullptr;
    float* delayBufferR = nullptr;

    // Não crítico: pode ficar em PSRAM (UI/assets/logs/cache).
    uint8_t* uiScratch = nullptr;
};

void applyParams(dsp::OrbitDelayCore& core, const AudioParams& p) {
    dsp::OrbitDelayCore::ReadMode coreReadMode = dsp::OrbitDelayCore::ReadMode::Orbit;
    switch (p.readMode) {
        case AudioParams::ReadMode::Accidental:
            coreReadMode = dsp::OrbitDelayCore::ReadMode::AccidentalReverse;
            break;
        case AudioParams::ReadMode::Orbit:
            coreReadMode = dsp::OrbitDelayCore::ReadMode::Orbit;
            break;
        default:
            coreReadMode = dsp::OrbitDelayCore::ReadMode::Orbit;
            break;
    }
    core.setReadMode(coreReadMode);

    core.setOrbit(p.orbit);
    core.setOffsetSamples(p.offsetSamples);
    core.setStereoSpread(p.stereoSpread);
    core.setFeedback(p.feedback);
    core.setMix(p.mix);
    core.setInputGain(p.inputGain);
    core.setOutputGain(p.outputGain);
    core.setToneHz(p.toneHz);
    core.setSmearAmount(p.smearAmount);
    core.setDiffuserStages(p.diffuserStages);
    core.setDcBlockEnabled(p.dcBlockEnabled);
}

void audioCallback(void* userData, const int32_t* inInterleaved, int32_t* outInterleaved, size_t frames) {
    auto* app = static_cast<AppContext*>(userData);

    AudioParams params;
    if (app->params.consumeIfUpdated(params)) {
        applyParams(app->core, params);
    }

    AudioSource& source = selectAudioSource(app->sourceType, app->externalI2sSource, app->internalTestSource);
    source.prepare(inInterleaved, frames);

    for (size_t i = 0; i < frames; ++i) {
        float inL = 0.0f;
        float inR = 0.0f;
        source.renderFrame(i, inL, inR);

        float outL = 0.0f;
        float outR = 0.0f;
        app->core.processSampleStereo(inL, inR, outL, outR);

        const float clampedL = std::clamp(outL, -1.0f, 1.0f);
        const float clampedR = std::clamp(outR, -1.0f, 1.0f);
        outInterleaved[i * 2] = static_cast<int32_t>(clampedL * 2147483647.0f);
        outInterleaved[i * 2 + 1] = static_cast<int32_t>(clampedR * 2147483647.0f);
    }
}

void uiTick(void* userData) {
    auto* app = static_cast<AppContext*>(userData);
    static uint32_t tick = 0;
    ++tick;

    AudioParams p;
    p.mix = 0.30f + 0.15f * ((tick / 120) % 2);
    p.feedback = 0.40f;
    p.toneHz = 6500.0f;
    p.smearAmount = 0.20f;
    app->params.publish(p);
}

} // namespace
} // namespace orbit::embedded

extern "C" void app_main(void) {
    using namespace orbit::embedded;

    static AppContext app;

    // Crítico para áudio/DSP: SRAM interna para latência previsível.
    app.delayBufferL = static_cast<float*>(heap_caps_malloc(sizeof(float) * kMaxDelaySamples,
                                                            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    app.delayBufferR = static_cast<float*>(heap_caps_malloc(sizeof(float) * kMaxDelaySamples,
                                                            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    // Não crítico: PSRAM para UI/assets/logs.
    app.uiScratch = static_cast<uint8_t*>(heap_caps_malloc(256 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!app.delayBufferL || !app.delayBufferR || !app.uiScratch) {
        ESP_LOGE(kTag, "Falha ao alocar buffers (SRAM ou PSRAM)");
        return;
    }

    app.core.attachBuffers(app.delayBufferL, app.delayBufferR, kMaxDelaySamples);
    app.core.reset(static_cast<float>(board::audio::kSampleRate));

    AudioParams initialParams;
    initialParams.readMode = AudioParams::ReadMode::Accidental;
    initialParams.dcBlockEnabled = true;
    app.params.publish(initialParams);

    AudioEngineEsp32::Config audioCfg;
    audioCfg.sampleRate = board::audio::kSampleRate;
    audioCfg.enableRx = true;
    audioCfg.enableTx = true;

    if (!app.audio.init(audioCfg, audioCallback, &app) || !app.audio.start()) {
        ESP_LOGE(kTag, "Falha ao iniciar engine de áudio");
        return;
    }

    UiTft::Config uiCfg;
    uiCfg.refreshPeriodMs = 33;
    uiCfg.core = 1;
    uiCfg.priority = 2;

    if (!app.ui.start(uiCfg, uiTick, &app)) {
        ESP_LOGW(kTag, "UI não iniciou; áudio segue ativo");
    }

    ESP_LOGI(kTag, "Sistema iniciado: core DSP, áudio I2S+DMA e UI em tasks separadas");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
