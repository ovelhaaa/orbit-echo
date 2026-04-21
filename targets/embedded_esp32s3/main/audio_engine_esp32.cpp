#include "audio_engine_esp32.h"

#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace orbit::embedded {
namespace {
constexpr const char* kTag = "audio_engine";

#ifndef I2S_MCLK_MULTIPLE_DEFAULT
#define I2S_MCLK_MULTIPLE_DEFAULT I2S_MCLK_MULTIPLE_256
#endif

#ifndef I2S_BITS_PER_CHAN_DEFAULT
#define I2S_BITS_PER_CHAN_DEFAULT I2S_BITS_PER_SAMPLE_32BIT
#endif
}

AudioEngineEsp32::AudioEngineEsp32() = default;

AudioEngineEsp32::~AudioEngineEsp32() {
    deinit();
}

bool AudioEngineEsp32::init(const Config& config, AudioCallback callback, void* userData) {
    if (initialized_) {
        return true;
    }

    config_ = config;
    if (config_.fixedMclkHz <= 0) {
        config_.fixedMclkHz = config_.sampleRate * 256;
    }
    callback_ = callback;
    userData_ = userData;
    taskENTER_CRITICAL(&statsMux_);
    stats_ = {};
    taskEXIT_CRITICAL(&statsMux_);

    const uint32_t blockMs = static_cast<uint32_t>((1000ULL * config_.dmaBufferFrames) /
                                                   static_cast<uint32_t>(config_.sampleRate > 0 ? config_.sampleRate : 1));
    const uint32_t timeoutMs = (blockMs * 2U) + 2U;
    ioTimeoutTicks_ = pdMS_TO_TICKS(timeoutMs < 4U ? 4U : (timeoutMs > 50U ? 50U : timeoutMs));

    i2s_mode_t mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER);
    if (config_.enableTx) {
        mode = static_cast<i2s_mode_t>(mode | I2S_MODE_TX);
    }
    if (config_.enableRx) {
        mode = static_cast<i2s_mode_t>(mode | I2S_MODE_RX);
    }

    i2s_config_t i2sCfg{};
    i2sCfg.mode = mode;
    i2sCfg.sample_rate = static_cast<uint32_t>(config_.sampleRate);
    i2sCfg.bits_per_sample = static_cast<i2s_bits_per_sample_t>(config_.bitsPerSample);
    i2sCfg.channel_format = config_.channelFormat;
    i2sCfg.communication_format = config_.commFormat;
    i2sCfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sCfg.dma_buf_count = config_.dmaBufferCount;
    i2sCfg.dma_buf_len = config_.dmaBufferFrames;
    i2sCfg.use_apll = config_.useApll;
    i2sCfg.tx_desc_auto_clear = true;
#if defined(ESP_IDF_VERSION)
    i2sCfg.fixed_mclk = 0;
    i2sCfg.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
    i2sCfg.fixed_mclk = config_.fixedMclkHz;
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
    i2sCfg.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
    i2sCfg.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;
#endif
#endif

    if (i2s_driver_install(config_.port, &i2sCfg, 0, nullptr) != ESP_OK) {
        ESP_LOGE(kTag, "Falha ao instalar driver I2S");
        return false;
    }

    i2s_pin_config_t pinCfg{};
    pinCfg.bck_io_num = config_.bclkGpio;
    pinCfg.ws_io_num = config_.wsGpio;
    pinCfg.data_out_num = config_.enableTx ? config_.doutGpio : I2S_PIN_NO_CHANGE;
    pinCfg.data_in_num = config_.enableRx ? config_.dinGpio : I2S_PIN_NO_CHANGE;
    // PCM1808 precisa de MCLK explícito (SCKI).
    pinCfg.mck_io_num = config_.mclkGpio;

    if (i2s_set_pin(config_.port, &pinCfg) != ESP_OK) {
        ESP_LOGE(kTag, "Falha ao configurar pinos I2S");
        i2s_driver_uninstall(config_.port);
        return false;
    }
    i2s_zero_dma_buffer(config_.port);

    interleavedSamplesPerBlock_ = static_cast<size_t>(config_.dmaBufferFrames) * channelsPerFrame_;
    const size_t bytes = interleavedSamplesPerBlock_ * sizeof(int32_t);

    if (config_.enableRx) {
        rxBuffer_ = static_cast<int32_t*>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (config_.enableTx) {
        txBuffer_ = static_cast<int32_t*>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    if ((config_.enableTx && !txBuffer_) || (config_.enableRx && !rxBuffer_)) {
        ESP_LOGE(kTag, "Falha ao alocar buffers críticos em SRAM interna");
        deinit();
        return false;
    }

    stoppedSignal_ = xSemaphoreCreateBinary();
    if (!stoppedSignal_) {
        ESP_LOGE(kTag, "Falha ao criar semáforo de sincronização da task de áudio");
        deinit();
        return false;
    }

    initialized_ = true;
    return true;
}

bool AudioEngineEsp32::start() {
    if (!initialized_ || running_) {
        return initialized_;
    }

    running_ = true;
    BaseType_t ok = xTaskCreatePinnedToCore(audioTaskEntry, "audio_i2s_task", config_.taskStackBytes, this,
                                            config_.taskPriority, &taskHandle_, config_.taskCore);
    if (ok != pdPASS) {
        running_ = false;
        taskHandle_ = nullptr;
        ESP_LOGE(kTag, "Falha ao criar task de audio");
        return false;
    }

    return true;
}

void AudioEngineEsp32::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (taskHandle_ && stoppedSignal_) {
        xSemaphoreTake(stoppedSignal_, portMAX_DELAY);
    }

    taskHandle_ = nullptr;
}

void AudioEngineEsp32::deinit() {
    stop();
    if (initialized_) {
        i2s_driver_uninstall(config_.port);
    }

    if (stoppedSignal_) {
        vSemaphoreDelete(stoppedSignal_);
        stoppedSignal_ = nullptr;
    }

    if (rxBuffer_) {
        heap_caps_free(rxBuffer_);
        rxBuffer_ = nullptr;
    }
    if (txBuffer_) {
        heap_caps_free(txBuffer_);
        txBuffer_ = nullptr;
    }

    initialized_ = false;
}

AudioEngineEsp32::Stats AudioEngineEsp32::stats() const {
    taskENTER_CRITICAL(&statsMux_);
    const Stats snapshot = stats_;
    taskEXIT_CRITICAL(&statsMux_);
    return snapshot;
}

void AudioEngineEsp32::audioTaskEntry(void* ctx) {
    static_cast<AudioEngineEsp32*>(ctx)->audioTaskLoop();
}

void AudioEngineEsp32::audioTaskLoop() {
    const size_t bytesPerBlock = interleavedSamplesPerBlock_ * sizeof(int32_t);
    while (running_) {
        size_t bytesRead = 0;
        esp_err_t rxErr = ESP_OK;
        if (config_.enableRx) {
            rxErr = i2s_read(config_.port, rxBuffer_, bytesPerBlock, &bytesRead, ioTimeoutTicks_);
            if (rxErr != ESP_OK) {
                taskENTER_CRITICAL(&statsMux_);
                ++stats_.rxErrors;
                taskEXIT_CRITICAL(&statsMux_);
            }
            if (bytesRead != bytesPerBlock) {
                taskENTER_CRITICAL(&statsMux_);
                ++stats_.rxShortBlocks;
                taskEXIT_CRITICAL(&statsMux_);
            }
            // Política de bloco fixo: callback só roda com bloco completo.
            if (rxErr != ESP_OK || bytesRead != bytesPerBlock) {
                if (config_.enableTx && txBuffer_) {
                    std::memset(txBuffer_, 0, bytesPerBlock);
                    size_t bytesWritten = 0;
                    const esp_err_t txErr =
                        i2s_write(config_.port, txBuffer_, bytesPerBlock, &bytesWritten, ioTimeoutTicks_);
                    if (txErr != ESP_OK) {
                        taskENTER_CRITICAL(&statsMux_);
                        ++stats_.txErrors;
                        taskEXIT_CRITICAL(&statsMux_);
                    } else if (bytesWritten != bytesPerBlock) {
                        taskENTER_CRITICAL(&statsMux_);
                        ++stats_.txShortWrites;
                        taskEXIT_CRITICAL(&statsMux_);
                    }
                }
                continue;
            }
        }

        size_t frames = (bytesRead / sizeof(int32_t)) / channelsPerFrame_;
        if (!config_.enableRx) {
            frames = static_cast<size_t>(config_.dmaBufferFrames);
        }
        if (config_.enableTx && txBuffer_) {
            std::memset(txBuffer_, 0, bytesPerBlock);
        }
        if (callback_ && frames == static_cast<size_t>(config_.dmaBufferFrames)) {
            taskENTER_CRITICAL(&statsMux_);
            ++stats_.callbackCalls;
            taskEXIT_CRITICAL(&statsMux_);
            callback_(userData_, config_.enableRx ? rxBuffer_ : nullptr, txBuffer_, frames);
        }

        if (config_.enableTx) {
            size_t bytesWritten = 0;
            const size_t txBytes = frames * channelsPerFrame_ * sizeof(int32_t);
            const esp_err_t txErr =
                i2s_write(config_.port, txBuffer_, txBytes, &bytesWritten, ioTimeoutTicks_);
            if (txErr != ESP_OK) {
                taskENTER_CRITICAL(&statsMux_);
                ++stats_.txErrors;
                taskEXIT_CRITICAL(&statsMux_);
            } else if (bytesWritten != txBytes) {
                taskENTER_CRITICAL(&statsMux_);
                ++stats_.txShortWrites;
                taskEXIT_CRITICAL(&statsMux_);
            }
        }
    }

    taskHandle_ = nullptr;
    if (stoppedSignal_) {
        xSemaphoreGive(stoppedSignal_);
    }
    vTaskDelete(nullptr);
}

} // namespace orbit::embedded
