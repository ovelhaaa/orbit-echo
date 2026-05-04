#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "hw/display.h"
#include "hw/input.h"
#include "parameter_bridge.h"

namespace orbit::embedded::ui {

enum class UiMode { Scroll, Edit };

enum class ParamKind { Arc, ValueOnly, Division };

struct MenuParameter {
    std::string shortName;
    std::string longName;
    float value;
    float minVal;
    float maxVal;
    float step;
    ParamKind kind = ParamKind::Arc;
};

class UserInterface {
public:
    UserInterface()
        : encoder_(board::encoder::kA, board::encoder::kB),
          enc_btn_(board::encoder::kSwitch),
          bypass_btn_(board::controls::kBypassButton) {}

    bool init(uint8_t* framebuffer) {
        if (!display_.init(framebuffer)) return false;

        encoder_available_ = board::encoder::kPresent;
        if (encoder_available_) {
            encoder_available_ = encoder_.init();
            if (encoder_available_) enc_btn_.init();
        }
        bypass_btn_.init();

        params_ = {
            {"MIX", "MIX", 0.35f, 0.0f, 1.0f, 0.05f, ParamKind::Arc},
            {"FB", "FEEDBACK", 0.35f, 0.0f, 1.0f, 0.05f, ParamKind::Arc},
            {"TM", "TEMPO BPM", 120.0f, 60.0f, 240.0f, 1.0f, ParamKind::ValueOnly},
            {"DIV", "DIVISAO", 3.0f, 0.0f, 5.0f, 1.0f, ParamKind::Division},
            {"OFF", "OFFSET", 0.5f, 0.0f, 1.0f, 0.01f, ParamKind::Arc},
            {"FOC", "FOCUS", 8000.0f, 1000.0f, 10000.0f, 100.0f, ParamKind::Arc},
            {"DST", "DIFUSAO STAGES", 2.0f, 0.0f, 2.0f, 1.0f, ParamKind::ValueOnly},
            {"DIF", "QTD DIFUSAO", 0.2f, 0.0f, 1.0f, 0.05f, ParamKind::Arc},
            {"SPR", "STEREO SPREAD", 0.0f, 0.0f, 1.0f, 0.05f, ParamKind::Arc},
        };

        drawUI();
        display_.update();
        return true;
    }

    void tick(ParameterBridge& bridge) {
        if (encoder_available_) enc_btn_.update();
        bypass_btn_.update();

        if (bypass_btn_.justPressed()) {
            bypassed_ = !bypassed_;
            updateBridge(bridge);
            drawUI();
            display_.update();
        }

        if (encoder_available_ && enc_btn_.justPressed()) {
            mode_ = (mode_ == UiMode::Scroll) ? UiMode::Edit : UiMode::Scroll;
            drawUI();
            display_.update();
        }

        int delta = encoder_available_ ? encoder_.getDelta() : 0;
        if (delta == 0) return;

        if (mode_ == UiMode::Scroll) {
            selected_idx_ = std::clamp(selected_idx_ + delta, 0, (int)params_.size() - 1);
        } else {
            MenuParameter& p = params_[selected_idx_];
            p.value = std::clamp(p.value + (delta * p.step), p.minVal, p.maxVal);
            if (selected_idx_ == 6) p.value = std::round(p.value);      // stages
            if (selected_idx_ == 3) p.value = std::round(p.value);      // division index
            updateBridge(bridge);
        }

        drawUI();
        display_.update();
    }

private:
    static constexpr const char* kDivisions[6] = {"1/2", "1/4", "1/4.", "1/8", "1/8.", "1/16"};
    static constexpr float kDivisionValues[6] = {2.0f, 1.0f, 1.5f, 0.5f, 0.75f, 0.25f};

    bool isDiffusionAmountEnabled() const { return static_cast<int>(std::round(params_[6].value)) > 0; }

    std::string valueText(int idx) const {
        const MenuParameter& p = params_[idx];
        char buf[20];
        if (idx == 3) return kDivisions[static_cast<int>(std::round(p.value))];
        if (idx == 6) return static_cast<int>(std::round(p.value)) == 0 ? "OFF" : std::to_string(static_cast<int>(std::round(p.value)));
        if (idx == 2) {
            std::snprintf(buf, sizeof(buf), "%.0f", p.value);
        } else {
            std::snprintf(buf, sizeof(buf), "%.2f", p.value);
        }
        return buf;
    }

    void drawArc(int cx, int cy, int r, float ratio, uint16_t color) {
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        const float start = 2.4f;
        const float end = 6.9f;
        for (int t = 0; t < 10; ++t) {
            for (int i = 0; i <= 100; ++i) {
                float p = static_cast<float>(i) / 100.0f;
                float a = start + (end - start) * p;
                int x = cx + static_cast<int>(std::cos(a) * (r - t));
                int y = cy + static_cast<int>(std::sin(a) * (r - t));
                display_.fillRect(x, y, 1, 1, hw::COLOR_DARK_GRAY);
                if (p <= ratio) display_.fillRect(x, y, 1, 1, color);
            }
        }
    }

    void updateBridge(ParameterBridge& bridge) {
        AudioParams ap;
        ap.mix = bypassed_ ? 0.0f : params_[0].value;
        ap.feedback = params_[1].value;
        ap.toneHz = params_[5].value;
        ap.orbit = 0.5f;
        ap.stereoSpread = params_[8].value;
        ap.diffuserStages = static_cast<uint32_t>(std::round(params_[6].value));
        ap.smearAmount = isDiffusionAmountEnabled() ? params_[7].value : 0.0f;

        float bpm = params_[2].value;
        int divIdx = static_cast<int>(std::round(params_[3].value));
        float noteDivision = kDivisionValues[divIdx];
        float tempoDelaySamples = (60.0f / bpm) * 48000.0f * noteDivision;
        ap.offsetSamples = tempoDelaySamples * params_[4].value;
        bridge.publish(ap);
    }

    void drawUI() {
        display_.clear(hw::COLOR_BLACK);
        const int headerH = 18;
        display_.fillSmoothCircle(8, 8, 5, bypassed_ ? hw::COLOR_DARK_GRAY : hw::COLOR_RED, hw::COLOR_BLACK);
        display_.drawText(16, 4, bypassed_ ? "BYPASS" : "ACTIVE", hw::COLOR_TEXT);
        display_.drawText(hw::Display::kWidth - 66, 4, "ORBIT ECHO", hw::COLOR_TEXT);
        display_.fillRect(0, headerH, hw::Display::kWidth, 1, hw::COLOR_DARK_GRAY);

        const int paneTop = headerH + 2;
        const int footerTop = hw::Display::kHeight - 12;
        const int paneH = (footerTop - paneTop) / 3;
        int start = std::clamp(selected_idx_ - 1, 0, (int)params_.size() - 3);

        for (int section = 0; section < 3; ++section) {
            int idx = start + section;
            int y0 = paneTop + section * paneH;
            int y1 = y0 + paneH - 1;
            bool selected = idx == selected_idx_;
            if (section > 0) display_.fillRect(0, y0, hw::Display::kWidth, 1, hw::COLOR_DARK_GRAY);
            if (selected) display_.fillRect(0, y0, hw::Display::kWidth, paneH, mode_ == UiMode::Edit ? hw::COLOR_RED : 0x0841);

            const MenuParameter& p = params_[idx];
            uint16_t fg = (selected && mode_ == UiMode::Edit) ? hw::COLOR_BLACK : hw::COLOR_TEXT;
            bool disabled = (idx == 7 && !isDiffusionAmountEnabled());
            if (disabled) fg = hw::COLOR_DARK_GRAY;

            if (p.kind == ParamKind::Arc && !disabled) {
                float ratio = (p.value - p.minVal) / (p.maxVal - p.minVal);
                drawArc(24, y0 + (paneH / 2), 15, ratio, selected && mode_ == UiMode::Edit ? hw::COLOR_BLACK : hw::COLOR_RED);
            }

            std::string val = valueText(idx);
            display_.drawText(56, y0 + 5, p.shortName.c_str(), fg);
            display_.drawText(56, y0 + 18, val.c_str(), fg);
            display_.drawText(8, y1 - 8, p.longName.c_str(), fg);
        }

        display_.fillRect(0, footerTop - 1, hw::Display::kWidth, 1, hw::COLOR_DARK_GRAY);
        display_.drawText(4, footerTop + 2, "ENC NAV / PUSH EDIT", hw::COLOR_TEXT);
    }

    hw::Display display_;
    hw::Encoder encoder_;
    hw::Button enc_btn_;
    hw::Button bypass_btn_;
    std::vector<MenuParameter> params_;
    int selected_idx_ = 0;
    UiMode mode_ = UiMode::Scroll;
    bool bypassed_ = false;
    bool encoder_available_ = false;
};

} // namespace orbit::embedded::ui
