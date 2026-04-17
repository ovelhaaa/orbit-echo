#pragma once

#include <cstdint>
#include <functional>
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace orbit::embedded::hw {

class Button {
public:
    Button(int gpio_num) : gpio_(static_cast<gpio_num_t>(gpio_num)) {}

    void init() {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << gpio_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    }

    void update() {
        bool current_state = (gpio_get_level(gpio_) == 0); // Active low with pull-up

        if (current_state != last_state_) {
            last_debounce_time_ = xTaskGetTickCount();
        }

        if ((xTaskGetTickCount() - last_debounce_time_) > pdMS_TO_TICKS(debounce_delay_ms_)) {
            if (current_state != state_) {
                state_ = current_state;
                if (state_) {
                    just_pressed_ = true;
                } else {
                    just_released_ = true;
                }
            }
        }
        last_state_ = current_state;
    }

    bool isPressed() const { return state_; }

    bool justPressed() {
        if (just_pressed_) {
            just_pressed_ = false;
            return true;
        }
        return false;
    }

    bool justReleased() {
        if (just_released_) {
            just_released_ = false;
            return true;
        }
        return false;
    }

private:
    gpio_num_t gpio_;
    bool state_ = false;
    bool last_state_ = false;
    bool just_pressed_ = false;
    bool just_released_ = false;
    TickType_t last_debounce_time_ = 0;
    const uint32_t debounce_delay_ms_ = 20;
};

class Encoder {
public:
    Encoder(int gpio_a, int gpio_b) : gpio_a_(gpio_a), gpio_b_(gpio_b) {}
    ~Encoder() {
        if (unit_) {
            pcnt_unit_stop(unit_);
            pcnt_unit_disable(unit_);
            pcnt_del_unit(unit_);
        }
    }

    bool init() {
        pcnt_unit_config_t unit_config = {
            .low_limit = -32768,
            .high_limit = 32767,
            .flags = {}
        };

        if (pcnt_new_unit(&unit_config, &unit_) != ESP_OK) {
            return false;
        }

        pcnt_glitch_filter_config_t filter_config = {
            .max_glitch_ns = 1000,
        };
        pcnt_unit_set_glitch_filter(unit_, &filter_config);

        pcnt_chan_config_t chan_config = {
            .edge_gpio_num = gpio_a_,
            .level_gpio_num = gpio_b_,
            .flags = {}
        };
        pcnt_channel_handle_t pcnt_chan = nullptr;
        if (pcnt_new_channel(unit_, &chan_config, &pcnt_chan) != ESP_OK) {
            return false;
        }

        pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
        pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

        pcnt_unit_enable(unit_);
        pcnt_unit_clear_count(unit_);
        pcnt_unit_start(unit_);
        return true;
    }

    int getDelta() {
        int count = 0;
        pcnt_unit_get_count(unit_, &count);
        int delta = count - last_count_;
        last_count_ = count;
        return delta;
    }

private:
    int gpio_a_;
    int gpio_b_;
    pcnt_unit_handle_t unit_ = nullptr;
    int last_count_ = 0;
};

} // namespace orbit::embedded::hw
