#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "parameter_bridge.h"

namespace orbit::embedded {

class UiTft {
public:
    struct Config {
        uint32_t refreshPeriodMs = 33;
        uint32_t stackWords = 4096;
        uint32_t priority = 3;
        int core = 1;
        ParameterBridge* paramBridge = nullptr;
    };

    using UiTickCallback = void (*)(void* userData);

    UiTft() = default;
    ~UiTft();

    bool start(const Config& config, UiTickCallback callback, void* userData);
    void stop();

private:
    static void taskEntry(void* ctx);
    void taskLoop();

    void processInputs();
    void updateState();
    void drawDisplay();

    Config config_{};
    UiTickCallback callback_ = nullptr;
    void* userData_ = nullptr;
    TaskHandle_t taskHandle_ = nullptr;
    SemaphoreHandle_t stoppedSignal_ = nullptr;
    bool running_ = false;

    // Encoder State
    int lastEncoderA_ = 1;
    int encoderPos_ = 0;
    bool switchPressed_ = false;
    bool lastSwitchState_ = true;
    uint32_t switchDebounceTime_ = 0;

    // Bypass State
    bool bypassPressed_ = false;
    bool isBypassed_ = false;
    bool lastBypassState_ = true;
    uint32_t bypassDebounceTime_ = 0;
    float preBypassMix_ = 0.35f;

    // UI Parameters
    enum class Page {
        Mix = 0,
        Feedback,
        Time,
        Tone,
        Count
    };
    Page currentPage_ = Page::Mix;

    AudioParams currentParams_;
};

} // namespace orbit::embedded
