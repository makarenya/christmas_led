#include "network.h"

#include <esp_system.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_wifi.h>

static const char *TAG = "NETWORK";
static std::string_view ApSsid = "christmas_led";

static void OnDisconnected(void *arg, esp_event_base_t, int32_t, void *event_data) {
    auto event = static_cast<system_event_sta_disconnected_t *>(event_data);
    auto callback = static_cast<INetworkCallback*>(arg);

    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    callback->OnWifiDisconnected();

    if (event->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
        esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    }
    ESP_ERROR_CHECK(esp_wifi_connect())
}

static void OnGotIp(void *arg, esp_event_base_t, int32_t, void *event_data) {
    auto event = static_cast<ip_event_got_ip_t *>(event_data);

    ESP_LOGI(TAG, "Connected with IPv4: " IPSTR, IP2STR(&event->ip_info.ip));

    auto callback = static_cast<INetworkCallback*>(arg);
    callback->OnWifiConnected(event->ip_info);
}

static void SoftAPEventHandler(void*, esp_event_base_t, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        auto* event = static_cast<wifi_event_ap_staconnected_t*>(event_data);
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        auto* event = static_cast<wifi_event_ap_stadisconnected_t*>(event_data);
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void StartSoftAP() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
    ESP_ERROR_CHECK(esp_wifi_init(&cfg))

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &SoftAPEventHandler, nullptr))

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM))
    wifi_config_t apConfig{};

    std::copy(ApSsid.begin(), ApSsid.end(), apConfig.ap.ssid);
    apConfig.ap.ssid_len = static_cast<uint8_t>(ApSsid.length());
    apConfig.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    apConfig.ap.max_connection = 4;
    apConfig.ap.authmode = WIFI_AUTH_OPEN;

    ESP_LOGI(TAG, "Starting AP...");
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &apConfig))
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP))
    ESP_ERROR_CHECK(esp_wifi_start())
}

void ConnectWifi(std::string_view ssid, std::string_view password, INetworkCallback* callback) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
    ESP_ERROR_CHECK(esp_wifi_init(&cfg))
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA))

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &OnDisconnected, static_cast<void*>(callback)))
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &OnGotIp, static_cast<void*>(callback)))

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM))

    wifi_config_t staConfig{};
    std::copy(ssid.begin(), ssid.end(), staConfig.sta.ssid);
    std::copy(password.begin(), password.end(), staConfig.sta.password);

    ESP_LOGI(TAG, "Connecting to %s...", staConfig.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &staConfig))
    ESP_ERROR_CHECK(esp_wifi_start())
    ESP_ERROR_CHECK(esp_wifi_connect())
}

