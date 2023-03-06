#pragma once

#include <esp_http_server.h>
#include <semphr.h>
#include "storage.h"

class TConfigServer {
public:
    TConfigServer(const TStorage* storage) : Storage_(storage), ConfigResult_(xSemaphoreCreateBinary()) {

    }
    void Start();
    void Stop();

    [[nodiscard]] std::string GetSsid() const {
        return Storage_->Get("ssid");
    }

    [[nodiscard]] std::string GetPassword() const {
        return Storage_->Get("password");
    }

    [[nodiscard]] bool Setup(std::string_view ssid, std::string_view password) const {
        if (!Storage_->Set("ssid", ssid)) {
            return false;
        }
        if (!Storage_->Set("password", password)) {
            return false;
        }
        if (!Storage_->Commit()) {
            return false;
        }
        return true;
    }

    [[nodiscard]] bool WaitResult() {
        return xSemaphoreTake(ConfigResult_, 3000);
    }

    void SendResult() {
        xSemaphoreGive(ConfigResult_);
    }

private:
    httpd_handle_t Handle_;
    const TStorage* Storage_;
    SemaphoreHandle_t ConfigResult_;
};