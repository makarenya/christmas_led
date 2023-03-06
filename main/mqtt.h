#pragma once
#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <string_view>
#include <mqtt_client.h>

class TMqttClient;

class IMqttCallback {
public:
    virtual ~IMqttCallback() = default;
    virtual void OnMqttConnected(const TMqttClient& client) = 0;
    virtual void OnMqttDisconnected(const TMqttClient& client) = 0;
    virtual void OnMqttSubscribed(const TMqttClient& client, std::string_view topic) = 0;
    virtual void OnMqttUnsubscribed(std::string_view topic) = 0;
    virtual void OnMqttData(const TMqttClient& client, std::string_view topic, std::string_view data) = 0;
};

class TMqttClient {
public:
    TMqttClient(std::string_view server, std::string_view username, std::string_view password, IMqttCallback* callback)
        : Server_(std::move(server)), Username_(std::move(username)), Password_(std::move(password)), Callback_(callback) {
    }
    ~TMqttClient();
    void Start();
    void Stop();
    void Publish(std::string_view topic, std::string_view data) const;
    void Subscribe(std::string_view topic) const;

private:
    esp_mqtt_client_handle_t Client_{nullptr};
    std::string Server_;
    std::string Username_;
    std::string Password_;
    IMqttCallback* Callback_;
};
