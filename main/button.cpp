#include "button.h"
#include "hardware.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
extern "C" {
#include <esp_task_wdt.h>
}

static QueueHandle_t ButtonQueue;

[[noreturn]] void ButtonTask(void* arg) {
    auto callback = static_cast<IButtonCallback*>(arg);
    bool pressed = false;
    size_t press_time = 0;
    while(true) {
        esp_task_wdt_reset();
        bool value;
        auto start = xTaskGetTickCount();
        if (xQueueReceive(ButtonQueue, &value, 3000) == pdTRUE) {
            if (press_time == 4) {
                callback->OnButtonReset();
            }
            press_time = 0;
            if (value) {
                pressed = true;
            } else {
                if (pressed && xTaskGetTickCount() - start > 2) {
                    callback->OnButtonNext();
                }
                pressed = false;
            }
        } else {
            if (press_time > 0) {
                ++press_time;
                if (press_time == 3) {
                    callback->OnButtonResetWindowBegin();
                } else if (press_time == 4) {
                    callback->OnButtonResetWindowEnd();
                }
            }
            if (pressed) {
                callback->OnButtonToggle();
                pressed = false;
                press_time = 1;
            }
        }
    }
}

void ButtonIsrHandler(void *) {
    bool value = !gpio_get_level(Button);
    xQueueSendFromISR(ButtonQueue, &value, nullptr);
}

void ButtonInit(IButtonCallback* callback) {
    gpio_config_t inputConf{
            BIT(Button),
            GPIO_MODE_INPUT,
            GPIO_PULLUP_ENABLE,
            GPIO_PULLDOWN_DISABLE,
            GPIO_INTR_POSEDGE
    };
    gpio_config(&inputConf);
    gpio_set_intr_type(Button, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(Button, ButtonIsrHandler, nullptr);

    ButtonQueue = xQueueCreate(6, sizeof(bool));
    xTaskCreate(ButtonTask, "ButtonTask", 2048, callback, 5, nullptr);
}