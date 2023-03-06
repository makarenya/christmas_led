#include "led.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <array>
#include <esp_log.h>
extern "C" {
#include <esp_task_wdt.h>
}

static QueueHandle_t LedQueue;

void LedSet(ELedState state) {
    xQueueSend(LedQueue, &state, 5);
}

std::array<uint16_t, 5> Patterns = {
        0xff00,
        0x8000,
        0xb800,
        0xbb80,
        0xbbb8
};

static_assert(static_cast<size_t>(ELedState::Candle) - static_cast<size_t>(ELedState::Sta) == Patterns.size() - 1);

[[noreturn]] void LedTask(void*) {
    ELedState persistent = ELedState::Off;
    ELedState state = ELedState::Off;
    uint32_t offset = 0;
    while (true) {
        esp_task_wdt_reset();
        ELedState cmd;
        if (xQueueReceive(LedQueue, &cmd, 100) == pdTRUE) {
            if (cmd == ELedState::Off || cmd == ELedState::Sta || cmd == ELedState::On) {
                persistent = cmd;
            }
            state = cmd;
            offset = 0;
        }
        if (state == ELedState::On) {
            gpio_set_level(Led, 0);
        } else if (state == ELedState::Off) {
            gpio_set_level(Led, 1);
        } else {
            bool level = !(Patterns[static_cast<uint32_t>(state) - static_cast<uint32_t>(ELedState::Sta)] << offset & 0x8000);
            gpio_set_level(Led, (persistent == ELedState::On) == !level);
        }
        if (++offset == 16) {
            if (state == ELedState::Sta) {
                offset = 0;
            } else {
                state = persistent;
            }
        }
    }
}

void LedInit() {
    gpio_config_t outputConf{
        BIT(Led),
        GPIO_MODE_OUTPUT,
        GPIO_PULLUP_DISABLE,
        GPIO_PULLDOWN_DISABLE,
        GPIO_INTR_DISABLE
    };
    gpio_config(&outputConf);

    LedQueue = xQueueCreate(6, sizeof(ELedState));
    xTaskCreate(LedTask, "LedTask", 256, nullptr, 5, nullptr);
}
