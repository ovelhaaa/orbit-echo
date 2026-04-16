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

        if (!encoder_.init()) return false;
        enc_btn_.init();
        bypass_btn_.init();

        // Initialize parameters
        params_.push_back({"Mix", 0.35f, 0.0f, 1.0f, 0.05f, "%"});
        params_.push_back({"Fback", 0.35f, 0.0f, 1.0f, 0.05f, "%"});
        params_.push_back({"Tone", 8000.0f, 1000.0f, 10000.0f, 100.0f, "Hz"});
        params_.push_back({"Smear", 0.0f, 0.0f, 1.0f, 0.05f, ""});
        params_.push_back({"Orbit", 0.5f, 0.0f, 1.0f, 0.05f, ""});
        params_.push_back({"Diff Stages", 2.0f, 1.0f, 4.0f, 1.0f, ""});

        display_.clear(hw::COLOR_BLACK);
        drawUI();
        display_.update();

        return true;
    }

    void tick(ParameterBridge& bridge) {
        enc_btn_.update();
        bypass_btn_.update();

        if (bypass_btn_.justPressed()) {
            bypassed_ = !bypassed_;
            updateBridge(bridge);
            drawUI();
            display_.update();
        }

        if (enc_btn_.justPressed()) {
            mode_ = (mode_ == UiMode::Scroll) ? UiMode::Edit : UiMode::Scroll;
            drawUI();
            display_.update();
        }

        int delta = encoder_.getDelta();
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
        ap.mix = bypassed_ ? 0.0f : params_[0].value;
        ap.feedback = params_[1].value;
        ap.toneHz = params_[2].value;
        ap.smearAmount = params_[3].value;
        ap.orbit = params_[4].value;
        ap.diffuserStages = static_cast<uint32_t>(params_[5].value);
        bridge.publish(ap);
    }

    void drawUI() {
        display_.clear(hw::COLOR_BLACK);

        // Header
        display_.fillRect(0, 0, hw::Display::kWidth, 16, hw::COLOR_ORBIT_BLUE);
        display_.drawText(4, 4, "Orbit Echo", hw::COLOR_WHITE);

        if (bypassed_) {
            display_.fillRect(hw::Display::kWidth - 50, 2, 46, 12, hw::COLOR_ACCENT);
            display_.drawText(hw::Display::kWidth - 46, 4, "BYPASS", hw::COLOR_WHITE);
        } else {
            display_.fillRect(hw::Display::kWidth - 30, 2, 26, 12, hw::COLOR_GREEN);
            display_.drawText(hw::Display::kWidth - 26, 4, "ON", hw::COLOR_BLACK);
        }

        // List
        int y_start = 24;
        int visible_items = 5;
        int start_idx = selected_idx_ - 2;
        if (start_idx < 0) start_idx = 0;
        if (start_idx > (int)params_.size() - visible_items) start_idx = params_.size() - visible_items;
        if (start_idx < 0) start_idx = 0;

        for (int i = 0; i < visible_items; i++) {
            int item_idx = start_idx + i;
            if (item_idx >= (int)params_.size()) break;

            int y = y_start + i * 20;
            bool is_selected = (item_idx == selected_idx_);

            if (is_selected) {
                if (mode_ == UiMode::Edit) {
                    display_.fillRect(2, y - 2, hw::Display::kWidth - 4, 18, hw::COLOR_ACCENT);
                } else {
                    display_.fillRect(2, y - 2, hw::Display::kWidth - 4, 18, hw::COLOR_GRAY);
                }
            }

            MenuParameter& p = params_[item_idx];

            // Name
            display_.drawText(6, y, p.name.c_str(), is_selected ? hw::COLOR_BLACK : hw::COLOR_WHITE);

            // Value text
            char val_str[16];
            if (p.step >= 1.0f) {
                snprintf(val_str, sizeof(val_str), "%.0f %s", p.value, p.unit.c_str());
            } else {
                snprintf(val_str, sizeof(val_str), "%.2f %s", p.value, p.unit.c_str());
            }
            display_.drawText(80, y, val_str, is_selected ? hw::COLOR_BLACK : hw::COLOR_WHITE);

            // Progress bar
            int bar_w = 80;
            int bar_h = 6;
            int bar_x = 150;
            int bar_y = y + 1;
            display_.fillRect(bar_x, bar_y, bar_w, bar_h, hw::COLOR_BLACK);
            float ratio = (p.value - p.min_val) / (p.max_val - p.min_val);
            display_.fillRect(bar_x + 1, bar_y + 1, (int)(ratio * (bar_w - 2)), bar_h - 2, hw::COLOR_ORBIT_BLUE);
        }
    }

    hw::Display display_;
    hw::Encoder encoder_;
    hw::Button enc_btn_;
    hw::Button bypass_btn_;

    std::vector<MenuParameter> params_;
    int selected_idx_ = 0;
    UiMode mode_ = UiMode::Scroll;
    bool bypassed_ = false;
};

} // namespace orbit::embedded::ui