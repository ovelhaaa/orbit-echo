#pragma once

#include <cstddef>
#include <cstdint>

#include "board_config.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace orbit::embedded {

class AudioEngineEsp32 {
public:
    struct Config {
        i2s_port_t port = I2S_NUM_0;
        int sampleRate = board::audio::kSampleRate;
        int bitsPerSample = board::audio::kI2sBitsPerSlot;
        int dmaBufferCount = board::audio::kDmaBufferCount;
        int dmaBufferFrames = board::audio::kDmaBufferFrames;
        i2s_comm_format_t commFormat = I2S_COMM_FORMAT_STAND_I2S;
        i2s_channel_fmt_t channelFormat = I2S_CHANNEL_FMT_RIGHT_LEFT;
        bool enableTx = true;
        bool enableRx = true;
        int mclkGpio = board::audio::i2s::kMclkGpio;
        int bclkGpio = board::audio::i2s::kBclkGpio;
        int wsGpio = board::audio::i2s::kLrckGpio;
        int doutGpio = board::audio::i2s::kDoutGpio;
        int dinGpio = board::audio::i2s::kDinGpio;
        int fixedMclkHz = board::audio::kSampleRate * 256;
        bool useApll = true;
        int taskPriority = configMAX_PRIORITIES - 2;
        int taskCore = 0;
        int taskStackWords = 4096;
    };

    struct Stats {
        uint32_t rxErrors = 0;
        uint32_t rxShortBlocks = 0;
        uint32_t txErrors = 0;
        uint32_t txShortWrites = 0;
        uint32_t callbackCalls = 0;
    };

    using AudioCallback = void (*)(void* userData, const int32_t* inInterleaved, int32_t* outInterleaved,
                                   size_t frames);

    AudioEngineEsp32();
    ~AudioEngineEsp32();

    bool init(const Config& config, AudioCallback callback, void* userData);
    bool start();
    void stop();
    void deinit();
    Stats stats() const;

private:
    static void audioTaskEntry(void* ctx);
    void audioTaskLoop();

    Config config_{};
    AudioCallback callback_ = nullptr;
    void* userData_ = nullptr;

    int32_t* rxBuffer_ = nullptr;
    int32_t* txBuffer_ = nullptr;
    size_t channelsPerFrame_ = 2;
    size_t interleavedSamplesPerBlock_ = 0;
    Stats stats_{};

    bool initialized_ = false;
    bool running_ = false;
    TaskHandle_t taskHandle_ = nullptr;
    SemaphoreHandle_t stoppedSignal_ = nullptr;
};

} // namespace orbit::embedded
