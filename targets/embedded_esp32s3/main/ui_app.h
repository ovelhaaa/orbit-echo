#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "hw/display.h"
#include "hw/input.h"
#include "parameter_bridge.h"

namespace orbit::embedded::ui {

enum class UiMode {
    Scroll,
    Edit
};

struct MenuParameter {
    std::string name;
    float value;
    float min_val;
    float max_val;
    float step;
    std::string unit;
};

class UserInterface {
public:
    UserInterface()
        : encoder_(board::encoder::kA, board::encoder::kB),
          enc_btn_(board::encoder::kSwitch),
          bypass_btn_(board::controls::kBypassButton) {}

    bool init(uint8_t* framebuffer) {
        if (!display_.init(framebuffer)) return false;

        encoder_available_ = encoder_.init();
        if (encoder_available_) {
            enc_btn_.init();
        }
        bypass_btn_.init();

        // Initialize parameters matching the requested grid layout
        params_.push_back({"MIX", 0.35f, 0.0f, 1.0f, 0.05f, ""});
        params_.push_back({"BPM", 120.0f, 60.0f, 240.0f, 1.0f, ""});
        params_.push_back({"FEEDBACK", 0.35f, 0.0f, 1.0f, 0.05f, ""});
        params_.push_back({"DIVISION", 1.0f, 0.25f, 4.0f, 0.25f, ""});
        params_.push_back({"OFFSET", 0.5f, 0.0f, 1.0f, 0.01f, ""});
        params_.push_back({"FOCUS", 8000.0f, 1000.0f, 10000.0f, 100.0f, ""});

        display_.clear(hw::COLOR_BLACK);
        drawUI();
        display_.update();

        return true;
    }

    void tick(ParameterBridge& bridge) {
        if (encoder_available_) {
            enc_btn_.update();
        }
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
        if (delta != 0) {
            if (mode_ == UiMode::Scroll) {
                selected_idx_ += delta;
                if (selected_idx_ < 0) selected_idx_ = 0;
                if (selected_idx_ >= (int)params_.size()) selected_idx_ = params_.size() - 1;
            } else if (mode_ == UiMode::Edit) {
                MenuParameter& p = params_[selected_idx_];
                p.value += delta * p.step;
                if (p.value < p.min_val) p.value = p.min_val;
                if (p.value > p.max_val) p.value = p.max_val;
                updateBridge(bridge);
            }
            drawUI();
            display_.update();
        }
    }

private:
    void updateBridge(ParameterBridge& bridge) {
        AudioParams ap;

        // params_ indices:
        // 0: MIX (0.0 to 1.0)
        // 1: BPM (60.0 to 240.0)
        // 2: FEEDBACK (0.0 to 1.0)
        // 3: DIVISION (0.25 to 4.0)
        // 4: OFFSET (0.0 to 1.0 - representing 0 to 1) -> we map this to offsetSamples in DSP
        // 5: FOCUS (1000.0 to 10000.0) -> we map this to toneHz

        ap.mix = bypassed_ ? 0.0f : params_[0].value;
        ap.feedback = params_[2].value;
        ap.toneHz = params_[5].value;

        // Emulating parameters that aren't mapped directly in the current UI layout
        // but were present in the original design
        ap.smearAmount = 0.2f;
        ap.orbit = 0.5f;
        ap.diffuserStages = 2;

        // Note: BPM, DIVISION, and OFFSET calculation for DSP is typically handled
        // by setting them in the AudioParams struct if the parameter bridge supports it,
        // or calculating offsetSamples based on BPM. The current AudioParams struct
        // supports offsetSamples, so we will do a simple mapping here for demonstration
        float tempoBpm = params_[1].value;
        float noteDivision = params_[3].value;
        float tempoDelaySamples = (60.0f / tempoBpm) * 48000.0f * noteDivision;
        ap.offsetSamples = tempoDelaySamples * params_[4].value;

        bridge.publish(ap);
    }

    void drawUI() {
        display_.clear(hw::COLOR_BLACK);

        // Header
    int header_h = 20;
    int led_x = 10;       // Centro do LED
    int led_y = 10;
    int text_x = 24;      // Texto travado no eixo X para evitar o "pulo"
    int text_y = 6;       // Altura do texto

    // System Active indicator (Red dot + text)
    if (bypassed_) {
        // LED "Apagado" - Dá um visual mais físico/industrial para a interface
        display_.fillSmoothCircle(led_x, led_y, 6, hw::COLOR_DARK_GRAY, hw::COLOR_BLACK);
        display_.drawText(text_x, text_y, "SYSTEM BYPASSED", hw::COLOR_TEXT);
    } else {
        // LED "Aceso"
        display_.fillSmoothCircle(led_x, led_y, 6, hw::COLOR_RED, hw::COLOR_BLACK); 
        display_.drawText(text_x, text_y, "SYSTEM ACTIVE", hw::COLOR_RED);
    }

    // Top right text
    display_.drawText(hw::Display::kWidth - 66, text_y, "ORBIT ECHO", hw::COLOR_TEXT);

    // Top horizontal line
    display_.fillRect(0, header_h, hw::Display::kWidth, 1, hw::COLOR_DARK_GRAY);
        // Top right text
        display_.drawText(hw::Display::kWidth - 66, 6, "ORBIT ECHO", hw::COLOR_TEXT);

        // Top horizontal line
        display_.fillRect(0, header_h, hw::Display::kWidth, 1, hw::COLOR_DARK_GRAY);

        // Grid layout calculations (2 columns, 3 rows)
        int cols = 2;
        int cell_w = (hw::Display::kWidth - 4) / cols;
        int cell_h = 32;
        int start_y = header_h + 4;

        // Draw center vertical separator
        display_.fillRect(hw::Display::kWidth / 2, start_y, 1, cell_h * 3, hw::COLOR_DARK_GRAY);

        for (int i = 0; i < (int)params_.size(); i++) {
            int col = i % cols;
            int row = i / cols;

            int x = 2 + col * cell_w;
            int y = start_y + row * cell_h;
            int inner_w = cell_w - 4;
            int inner_h = cell_h - 4;

            bool is_selected = (i == selected_idx_);

            // Draw selection box or background
            if (is_selected) {
                if (mode_ == UiMode::Edit) {
                    display_.fillRect(x, y, inner_w, inner_h, hw::COLOR_RED);
                } else {
                    // Draw white/gray border
                    display_.fillRect(x, y, inner_w, 1, hw::COLOR_BORDER);
                    display_.fillRect(x, y + inner_h - 1, inner_w, 1, hw::COLOR_BORDER);
                    display_.fillRect(x, y, 1, inner_h, hw::COLOR_BORDER);
                    display_.fillRect(x + inner_w - 1, y, 1, inner_h, hw::COLOR_BORDER);
                }
            }

            MenuParameter& p = params_[i];

            // Name Text (Black if editing, otherwise Text color)
            uint16_t name_color = (is_selected && mode_ == UiMode::Edit) ? hw::COLOR_BLACK : hw::COLOR_TEXT;
            display_.drawText(x + 4, y + 4, p.name.c_str(), name_color);

            // Custom Slider visualization
            int slider_x = x + 4;
            int slider_y = y + 18;
            int slider_w = inner_w - 8;
            int slider_h = 6;

            float ratio = (p.value - p.min_val) / (p.max_val - p.min_val);
            int fill_w = (int)(ratio * slider_w);

            // Background track
            display_.fillRect(slider_x, slider_y, slider_w, slider_h, hw::COLOR_DARK_GRAY);

            // Red fill (or black if editing and background is red)
            uint16_t fill_color = (is_selected && mode_ == UiMode::Edit) ? hw::COLOR_BLACK : hw::COLOR_RED;
            display_.fillRect(slider_x, slider_y, fill_w, slider_h, fill_color);

            // Cursor (Square white thumb)
            int cursor_w = 6;
            int cursor_h = 10;
            int cursor_x = slider_x + fill_w - (cursor_w / 2);
            if (cursor_x < slider_x) cursor_x = slider_x;
            if (cursor_x + cursor_w > slider_x + slider_w) cursor_x = slider_x + slider_w - cursor_w;
            int cursor_y = slider_y - 2;

            display_.fillRect(cursor_x, cursor_y, cursor_w, cursor_h, hw::COLOR_WHITE);
        }

        // Horizontal line above footer
        int footer_y = hw::Display::kHeight - 12;
        display_.fillRect(0, footer_y - 2, hw::Display::kWidth, 1, hw::COLOR_DARK_GRAY);

        // Footer
        display_.drawText(4, footer_y, "ENC: NAVIGATE", hw::COLOR_TEXT);

        // 3 colored dots in middle of footer
        int dots_x = (hw::Display::kWidth / 2) - 10;
        display_.fillRect(dots_x, footer_y + 2, 4, 4, hw::COLOR_RED);
        display_.fillRect(dots_x + 8, footer_y + 2, 4, 4, hw::COLOR_DARK_GRAY);
        display_.fillRect(dots_x + 16, footer_y + 2, 4, 4, hw::COLOR_DARK_GRAY);

        display_.drawText(hw::Display::kWidth - 64, footer_y, "PUSH: EDIT", hw::COLOR_TEXT);
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
