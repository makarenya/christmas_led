#include "output.h"

#include "modes.h"
#include "hardware.h"

#include <random>
#include <vector>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/pwm.h>

extern "C" {
#include <esp_task_wdt.h>
}

static QueueHandle_t ControlQueue;

[[noreturn]] void OutputTask(void *arg) noexcept {
    auto callback = static_cast<IOutputCallback *>(arg);
    std::mt19937 generator(esp_random());
    std::vector<std::shared_ptr<IMode>> modes;
    modes.emplace_back(CreateStatic());
    modes.emplace_back(CreateDynamic(1000));
    modes.emplace_back(CreateFireplace());
    modes.emplace_back(CreateCandle());
    bool isOn = false;
    uint32_t current = 0;

    while(true) {
        esp_task_wdt_reset();
        EOutputState cmd;
        TickType_t lastWakeTime;
        if (xQueueReceive(ControlQueue, &cmd, 5)) {
            switch (cmd) {
                case EOutputState::Off:
                    isOn = false;
                    callback->OnOutputChanged(isOn, current);
                    break;
                case EOutputState::On:
                    isOn = true;
                    callback->OnOutputChanged(isOn, current);
                    break;
                case EOutputState::Toggle:
                    isOn = !isOn;
                    callback->OnOutputChanged(isOn, current);
                    break;
                case EOutputState::Next:
                    if (!isOn) {
                        isOn = true;
                    } else {
                        current = current == modes.size() - 1 ? 0 : current + 1;
                    }
                    callback->OnOutputChanged(isOn, current);
                    break;
                case EOutputState::Static:
                case EOutputState::Dynamic:
                case EOutputState::Fireplace:
                case EOutputState::Candle: {
                    current = static_cast<uint32_t>(cmd) - static_cast<uint32_t>(EOutputState::Static);
                    isOn = true;
                    callback->OnOutputChanged(isOn, current);
                    break;
                }
                case EOutputState::Unknown:
                    break;
            }
        }
        vTaskDelayUntil(&lastWakeTime, 10);
        auto value = isOn ? modes[current]->step(generator) : 0.0;
        //ESP_LOGI("VAL", "Value: %ld", static_cast<long>(value * 10000));
        if (value > 1.0) {
            value = 1.0;
        } else if (value < 0.0) {
            value = 0.0;
        }
        pwm_set_duty(0, static_cast<uint32_t>(value * value * 1000));
        pwm_start();
    }
}

void OutputSet(EOutputState command) {
    xQueueSend(ControlQueue, &command, 5);
}

void OutputInit(IOutputCallback* callback) {
    uint32_t pwm_nums = Output;
    uint32_t duties = 0;

    pwm_init(1000, &duties, 1, &pwm_nums);
    pwm_set_phase(0, 0);
    pwm_start();

    ControlQueue = xQueueCreate(6, sizeof(EOutputState));
    xTaskCreate(OutputTask, "OutputTask", 4096, callback, 5, nullptr);
}
