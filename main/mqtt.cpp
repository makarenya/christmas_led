#include "mqtt.h"

#include <esp_log.h>
#include <mqtt_client.h>

static const char *TAG = "MQTT";

static void MqttEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

void TMqttClient::Start() {
    esp_mqtt_client_config_t config {
        .uri = Server_.data(),
        .username = Username_.data(),
        .password = Password_.data(),
        .user_context = static_cast<void *>(Callback_),
    };
    Client_ = esp_mqtt_client_init(&config);
    esp_mqtt_client_register_event(Client_, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), MqttEventHandler, this);
    ESP_LOGI(TAG, "Starting...");
    esp_mqtt_client_start(Client_);
}

void TMqttClient::Stop() {
    if (Client_ != nullptr) {
        ESP_LOGI(TAG, "Stopping...");
        esp_mqtt_client_stop(Client_);
        esp_mqtt_client_destroy(Client_);
        Client_ = nullptr;
        ESP_LOGI(TAG, "Stopped");
    }
}

void TMqttClient::Publish(std::string_view topic, std::string_view data) const {
    esp_mqtt_client_publish(Client_, topic.data(), data.data(), static_cast<int>(data.size()), 0, 0);
}

void TMqttClient::Subscribe(std::string_view topic) const {
    auto msg_id = esp_mqtt_client_subscribe(Client_, topic.data(), 0);
    ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
}

TMqttClient::~TMqttClient() {
    ESP_LOGI(TAG, "Destroying");
    esp_mqtt_client_stop(Client_);
    esp_mqtt_client_destroy(Client_);
}

static void MqttEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    auto event = static_cast<esp_mqtt_event_handle_t>(event_data);
    auto client = static_cast<TMqttClient *>(handler_args);
    auto callback = static_cast<IMqttCallback *>(event->user_context);
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            callback->OnMqttConnected(*client);
            break;
        case MQTT_EVENT_DISCONNECTED:
            callback->OnMqttDisconnected(*client);
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            callback->OnMqttSubscribed(*client, {event->topic, static_cast<size_t>(event->topic_len)});
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            callback->OnMqttUnsubscribed({event->topic, static_cast<size_t>(event->topic_len)});
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            callback->OnMqttData(*client, {event->topic, static_cast<size_t>(event->topic_len)}, {event->data, static_cast<size_t>(event->data_len)});
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
                ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}