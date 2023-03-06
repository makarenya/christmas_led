/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "storage.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_event.h>
extern "C" {
#include <esp_task_wdt.h>
}

#include "captive.h"
#include "network.h"
#include "mqtt.h"
#include "modes.h"
#include "config.h"
#include "led.h"
#include "button.h"
#include "output.h"
#include "private.h"

static const char *TAG = "CRISTMAS_LED";
static TStorage ConfigStorage;

class TController : public IMqttCallback, public INetworkCallback, public IButtonCallback, public IOutputCallback {
public:
    TController() : MqttClient_(MqttServer, MqttLogin, MqttPassword, this), Connected_(false) {
    }

    void OnWifiConnected(const tcpip_adapter_ip_info_t& info) override {
        ESP_LOGI(TAG, "Wifi Connected");
        MqttClient_.Start();
    }

    void OnWifiDisconnected() override {
        ESP_LOGI(TAG, "MQTT Disconnected");
        LedSet(ELedState::On);
        Connected_ = false;
        MqttClient_.Stop();
    }

    void OnMqttConnected(const TMqttClient& client) override {
        ESP_LOGI(TAG, "MQTT Connected");
        client.Subscribe("/alexx/christmas_led/state");
        client.Subscribe("/alexx/christmas_led/control");
    }

    void OnMqttDisconnected(const TMqttClient& client) override {
        ESP_LOGI(TAG, "MQTT Disconnected");
        LedSet(ELedState::On);
        Connected_ = false;
    }

    void OnMqttSubscribed(const TMqttClient& client, std::string_view topic) override {
        ESP_LOGI(TAG, "MQTT Subscribed");
        LedSet(ELedState::Off);
        Connected_ = true;
        if (topic == "/alexx/led/state") {
            client.Publish(topic, "off");
        }
    }

    void OnMqttUnsubscribed(std::string_view topic) override {
        ESP_LOGI(TAG, "MQTT Unsubscribed");
    }

    void OnMqttData(const TMqttClient& client, std::string_view topic, std::string_view data) override {
        if (topic != "/alexx/christmas_led/control") {
            return;
        }
        if (data == "on" || data == "true" || data == "1") {
            ESP_LOGI(TAG, "Switch On by MQTT");
            OutputSet(EOutputState::On);
        } else if (data == "off" || data == "false" || data == "0") {
            ESP_LOGI(TAG, "Switch Off by MQTT");
            OutputSet(EOutputState::Off);
        } else if (data == "toggle") {
            ESP_LOGI(TAG, "Toggle by MQTT");
            OutputSet(EOutputState::Toggle);
        } else if (data == "static") {
            ESP_LOGI(TAG, "Static mode by MQTT");
            OutputSet(EOutputState::Static);
        } else if (data == "dynamic") {
            ESP_LOGI(TAG, "Dynamic mode by MQTT");
            OutputSet(EOutputState::Dynamic);
        } else if (data == "fireplace") {
            ESP_LOGI(TAG, "Fireplace mode by MQTT");
            OutputSet(EOutputState::Fireplace);
        } else if (data == "candle") {
            ESP_LOGI(TAG, "Candle mode by MQTT");
            OutputSet(EOutputState::Candle);
        }
    }

    void OnButtonToggle() override {
        ESP_LOGI(TAG, "Toggle by button");
        OutputSet(EOutputState::Toggle);
    }

    void OnButtonNext() override {
        ESP_LOGI(TAG, "Next mode by button");
        OutputSet(EOutputState::Next);
    }

    void OnButtonResetWindowBegin() override {
        ESP_LOGI(TAG, "Waiting reset...");
        LedSet(ELedState::On);
    }

    void OnButtonResetWindowEnd() override {
        ESP_LOGI(TAG, "Ready to reset...");
        LedSet(ELedState::Off);
    }

    void OnButtonReset() override {
        ESP_LOGI(TAG, "Erase and reset");
        ConfigStorage.Erase();
        esp_restart();
    }

    void OnOutputChanged(bool isOn, size_t mode) override {
        if (Connected_) {
            if (isOn && mode == 0) {
                ESP_LOGI(TAG, "Switched to static");
                MqttClient_.Publish("/alexx/led/state", "on");
            } else if (isOn && mode == 1) {
                ESP_LOGI(TAG, "Switched to dynamic");
                MqttClient_.Publish("/alexx/led/state", "dynamic");
            } else if (isOn && mode == 2) {
                ESP_LOGI(TAG, "Switched to fireplace");
                MqttClient_.Publish("/alexx/led/state", "fireplace");
            } else if (isOn && mode == 3) {
                ESP_LOGI(TAG, "Switched to candle");
                MqttClient_.Publish("/alexx/led/state", "candle");
            } else {
                ESP_LOGI(TAG, "Switched off");
                MqttClient_.Publish("/alexx/led/state", "off");
            }
        }
        if (isOn) {
            LedSet(static_cast<ELedState>(mode + static_cast<size_t>(ELedState::Static)));
        }
    }

private:
    TMqttClient MqttClient_;
    bool Connected_;
};

static TController Controller;
static TConfigServer ConfigServer(&ConfigStorage);

extern "C" {

[[maybe_unused]] void app_main() {
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    ESP_ERROR_CHECK(nvs_flash_init())
    ESP_ERROR_CHECK(esp_netif_init())
    ESP_ERROR_CHECK(esp_event_loop_create_default())

    LedInit();
    ButtonInit(&Controller);
    OutputInit(&Controller);

    ConfigStorage.Init("config");
    if (ConfigServer.GetSsid().empty()) {
        LedSet(ELedState::Sta);
        StartSoftAP();
        ConfigServer.Start();
        start_dns_server();
    } else {
        LedSet(ELedState::On);
        ConnectWifi(ConfigServer.GetSsid(), ConfigServer.GetPassword(), &Controller);
    }
}

}
