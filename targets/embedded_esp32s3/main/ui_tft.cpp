#include "ui_tft.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "board_config.h"

#include <algorithm>
#include <cmath>

namespace orbit::embedded {
namespace {
constexpr const char* kTag = "ui_tft";
}

UiTft::~UiTft() {
    stop();
}

bool UiTft::start(const Config& config, UiTickCallback callback, void* userData) {
    if (running_) {
        return true;
    }

    config_ = config;
    callback_ = callback;
    userData_ = userData;

    // Configuração dos GPIOs para o Encoder e Botão Bypass
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << board::encoder::kA) |
                           (1ULL << board::encoder::kB) |
                           (1ULL << board::encoder::kSwitch) |
                           (1ULL << board::controls::kBypassButton);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Incializa o estado do encoder
    lastEncoderA_ = gpio_get_level(static_cast<gpio_num_t>(board::encoder::kA));

    if (!stoppedSignal_) {
        stoppedSignal_ = xSemaphoreCreateBinary();
        if (!stoppedSignal_) {
            ESP_LOGE(kTag, "Falha ao criar semáforo de sincronização da UI");
            return false;
        }
    }

    running_ = true;

    BaseType_t ok = xTaskCreatePinnedToCore(taskEntry, "ui_tft_task", config_.stackWords, this,
                                             config_.priority, &taskHandle_, config_.core);

    if (ok != pdPASS) {
        running_ = false;
        taskHandle_ = nullptr;
        ESP_LOGE(kTag, "Falha ao criar task de UI");
        return false;
    }

    return true;
}

void UiTft::stop() {
    if (!running_) {
        if (stoppedSignal_) {
            vSemaphoreDelete(stoppedSignal_);
            stoppedSignal_ = nullptr;
        }
        return;
    }

    running_ = false;

    if (taskHandle_ && stoppedSignal_) {
        xSemaphoreTake(stoppedSignal_, portMAX_DELAY);
    }

    taskHandle_ = nullptr;

    if (stoppedSignal_) {
        vSemaphoreDelete(stoppedSignal_);
        stoppedSignal_ = nullptr;
    }
}

void UiTft::taskEntry(void* ctx) {
    static_cast<UiTft*>(ctx)->taskLoop();
}

void UiTft::processInputs() {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Encoder - Leitura baseada em polling de quadratura
    int a = gpio_get_level(static_cast<gpio_num_t>(board::encoder::kA));
    int b = gpio_get_level(static_cast<gpio_num_t>(board::encoder::kB));

    if (a != lastEncoderA_) {
        if (a == 0) { // Na borda de descida de A
            if (b == 1) {
                encoderPos_++; // CW
            } else {
                encoderPos_--; // CCW
            }
        }
        lastEncoderA_ = a;
    }

    // Encoder Switch (Debounced)
    bool switchState = gpio_get_level(static_cast<gpio_num_t>(board::encoder::kSwitch));
    if (switchState != lastSwitchState_) {
        if (now - switchDebounceTime_ > 50) { // 50ms debounce
            if (switchState == 0) { // Pressionado (pull-up)
                switchPressed_ = true;
            }
            lastSwitchState_ = switchState;
            switchDebounceTime_ = now;
        }
    }

    // Bypass Button (Debounced)
    bool bypassState = gpio_get_level(static_cast<gpio_num_t>(board::controls::kBypassButton));
    if (bypassState != lastBypassState_) {
        if (now - bypassDebounceTime_ > 50) { // 50ms debounce
            if (bypassState == 0) { // Pressionado (pull-up)
                bypassPressed_ = true;
            }
            lastBypassState_ = bypassState;
            bypassDebounceTime_ = now;
        }
    }
}

void UiTft::updateState() {
    bool paramsUpdated = false;

    // Trata o botão Bypass
    if (bypassPressed_) {
        isBypassed_ = !isBypassed_;
        bypassPressed_ = false;

        // Em caso de bypass, podemos mudar o readMode ou zerar o mix.
        // Aqui assumimos zerar o mix como forma de bypass.
        if (isBypassed_) {
            preBypassMix_ = currentParams_.mix;
            currentParams_.mix = 0.0f;
        } else {
            currentParams_.mix = preBypassMix_;
        }
        paramsUpdated = true;
    }

    // Trata o botão do Encoder (Muda a página)
    if (switchPressed_) {
        int nextPage = static_cast<int>(currentPage_) + 1;
        if (nextPage >= static_cast<int>(Page::Count)) {
            nextPage = 0;
        }
        currentPage_ = static_cast<Page>(nextPage);
        switchPressed_ = false;
    }

    // Trata a rotação do Encoder (Muda os valores da página atual)
    if (encoderPos_ != 0) {
        if (!isBypassed_) { // Ignora rotações no bypass, ou desabilita bypass?
            switch (currentPage_) {
                case Page::Mix:
                    currentParams_.mix = std::clamp(currentParams_.mix + (encoderPos_ * 0.05f), 0.0f, 1.0f);
                    break;
                case Page::Feedback:
                    currentParams_.feedback = std::clamp(currentParams_.feedback + (encoderPos_ * 0.05f), 0.0f, 1.0f);
                    break;
                case Page::Time:
                    // 24000.0f is kMaxDelaySamples. It would be better to share it, but for now we clamp against it.
                    currentParams_.offsetSamples = std::clamp(currentParams_.offsetSamples + (encoderPos_ * 1000.0f), 0.0f, 24000.0f);
                    break;
                case Page::Tone:
                    currentParams_.toneHz = std::clamp(currentParams_.toneHz + (encoderPos_ * 500.0f), 200.0f, 20000.0f);
                    break;
                default:
                    break;
            }
            paramsUpdated = true;
        }
        encoderPos_ = 0;
    }

    // Publica se houve mudança real e o ParameterBridge estiver configurado
    if (paramsUpdated && config_.paramBridge) {
        config_.paramBridge->publish(currentParams_);
    }
}

void UiTft::drawDisplay() {
    // Stub: Simular o desenho do display
    // Em uma implementação real usaria SPI para enviar os dados da tela
}


void UiTft::taskLoop() {
    const TickType_t periodTicks = pdMS_TO_TICKS(config_.refreshPeriodMs);
    TickType_t lastWake = xTaskGetTickCount();

    // Sincroniza parâmetros iniciais
    if (config_.paramBridge) {
       // Se o paramBridge tivesse método getter, a gente puxava.
       // Vamos inicializar com alguns valores padrão conhecidos,
       // alinhados com o estado default da struct AudioParams
       currentParams_.mix = 0.35f;
       currentParams_.feedback = 0.35f;
       currentParams_.offsetSamples = 1200.0f;
       currentParams_.toneHz = 8000.0f;
    }

    while (running_) {
        processInputs();
        updateState();
        drawDisplay();

        if (callback_) {
            callback_(userData_);
        }

        vTaskDelayUntil(&lastWake, periodTicks);
    }

    taskHandle_ = nullptr;
    if (stoppedSignal_) {
        xSemaphoreGive(stoppedSignal_);
    }
    vTaskDelete(nullptr);
}

} // namespace orbit::embedded
