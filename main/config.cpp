#include "config.h"

#include <esp_log.h>
#include <string_view>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <array>
#include <nvs.h>
#include <esp_wifi.h>

static const char *TAG="APP";
extern const uint8_t LoginHtmlStart[] asm("_binary_login_html_start");
extern const uint8_t SuccessHtmlStart[] asm("_binary_success_html_start");

namespace NInternal {
    template<typename TValuesHolder>
    std::string ProcessTemplate(std::string_view source, TValuesHolder valuesHolder) {
        std::string result;
        result.reserve(source.size() + 100);
        auto keyStart = source.end();
        auto keyState = 0;

        for (auto ch = source.begin(); ch != source.end(); ++ch) {
            if (keyState == 0) {
                if (*ch == '{') {
                    keyStart = ch;
                    keyState = 1;
                } else {
                    result += *ch;
                }
            } else if (keyState == 1) {
                if (*ch != '{') {
                    result += std::string_view(keyStart, ch - keyStart + 1);
                    keyState = 0;
                } else {
                    keyState = 2;
                }
            } else if (keyState == 2) {
                if (*ch == '}') {
                    keyState = 3;
                }
            } else if (*ch != '}') {
                result += std::string_view(keyStart, ch - keyStart + 1);
                keyState = 0;
            } else {
                result += valuesHolder(std::string_view(keyStart + 2, ch - keyStart - 3));
                keyStart = source.end();
                keyState = 0;
            }
        }
        if (keyState != 0) {
            result += std::string_view(keyStart, source.end() - keyStart);
        }
        return result;
    }

    std::string EncodeHtml(std::string_view text) {
        std::string result;
        result.reserve(text.size() * 2);
        for (char ch : text) {
            switch (ch) {
                case '&':
                    result += "&amp;";
                    break;
                case '\'':
                    result += "&apos;";
                    break;
                case '"':
                    result += "&quot;";
                    break;
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                default:
                    result += ch;
            }
        }
        return result;
    }

    class TRequestIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = char;
        using pointer           = char*;  // or also value_type*
        using reference         = char&;  // or also value_type&

        explicit TRequestIterator(httpd_req_t *req)
                : Request_(req)
                , Remaining_(req->content_len)
                , Portion_(0)
                , Offset_(0) {
            FetchNext();
        }

        TRequestIterator() : Request_(nullptr), Remaining_(0), Portion_(0), Offset_(0) {
        }

        char operator*() const {
            return Buf_[Offset_];
        }

        TRequestIterator& operator++() {
            if (!IsEmpty()) {
                if (++Offset_ == Portion_) {
                    FetchNext();
                }
            }
            return *this;
        }

        bool operator != (const TRequestIterator& other) {
            if (IsEmpty() && other.IsEmpty()) {
                return false;
            }
            return Request_ != other.Request_ || Remaining_ != other.Remaining_ || Offset_ != other.Offset_;
        }


    private:

        [[nodiscard]] bool IsEmpty() const {
            return Remaining_ == 0 && Offset_ == Portion_;
        }

        void FetchNext() {
            if (Remaining_ == 0) {
                return;
            }
            int ret;
            do {
                ret = httpd_req_recv(Request_, Buf_.data(), std::min(Remaining_, Buf_.size()));
            } while (ret == HTTPD_SOCK_ERR_TIMEOUT);
            if (ret < 0) {
                Remaining_ = 0;
            } else {
                Remaining_ -= ret;
            }
            Offset_ = 0;
            Portion_ = ret;
        }

    private:
        httpd_req_t *Request_;
        size_t Remaining_;
        std::array<char, 64> Buf_;
        size_t Portion_;
        size_t Offset_;
    };

    template<typename TIterator>
    std::unordered_map<std::string, std::string> ReadUrlEncoded(TIterator begin, TIterator end) {
        std::unordered_map<std::string, std::string> result;
        bool percentMode = false;
        std::string percentValue;
        std::string key;
        std::string value;
        bool keyMode = true;
        for (auto ch = begin; ch != end; ++ch) {
            if (keyMode && *ch == '=') {
                keyMode = false;
            } else if (keyMode) {
                key += *ch;
            } else if (percentMode) {
                percentValue += *ch;
                if (percentValue.size() == 2) {
                    value += static_cast<char>(std::stoul(percentValue, nullptr, 16));
                    percentValue.clear();
                    percentMode = false;
                }
            } else if (*ch == '%') {
                percentMode = true;
            } else if (*ch == '&') {
                keyMode = true;
                result.emplace(key, value);
                key.clear();
                value.clear();
            } else {
                value += *ch;
            }
        }
        if (!key.empty()) {
            result.emplace(key, value);
        }
        return result;
    }
}

struct TConfigState {
    nvs_handle Nvs;
    std::function<void()> Callback;
};

esp_err_t ConfigGetHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Got Handler");
    std::string_view response (reinterpret_cast<const char *>(LoginHtmlStart));
    ESP_LOGI(TAG, "Response %d", response.size());
    auto server = static_cast<TConfigServer*>(req->user_ctx);
    std::string result = NInternal::ProcessTemplate(response, [server](std::string_view key)-> std::string {
        if (key == "SSID") {
            return NInternal::EncodeHtml(server->GetSsid());
        }
        return {};
    });
    httpd_resp_send(req, result.data(), static_cast<ssize_t>(result.size()));
    return ESP_OK;
}

void CheckGotIp(void* event_handler_arg,
                esp_event_base_t event_base,
                int32_t event_id,
                void* event_data) {
    auto server = static_cast<TConfigServer *>(event_handler_arg);
    server->SendResult();
}

esp_err_t ConfigSaveHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Save Handler");
    using namespace NInternal;
    if (req->content_len > 512) {
        for (auto ch = TRequestIterator{req}; ch != TRequestIterator{}; ++ch) {
        }
        httpd_resp_set_status   (req, "413 Payload Too Large");
        httpd_resp_set_type     (req, HTTPD_TYPE_TEXT);
        std::string_view response = "Payload too large";
        httpd_resp_send(req, response.data(), static_cast<ssize_t>(response.size()));
        return ESP_OK;
    }

    auto values = ReadUrlEncoded(NInternal::TRequestIterator{req}, NInternal::TRequestIterator{});
    for (auto& kvp: values) {
        ESP_LOGI(TAG, "Got '%s' = '%s'", kvp.first.data(), kvp.second.data());
    }
    auto ssid = values.find("ssid");
    auto password = values.find("password");
    if (ssid == values.end() || password == values.end()) {
        httpd_resp_set_status   (req, "400 Bad Request");
        httpd_resp_set_type     (req, HTTPD_TYPE_TEXT);
        std::string_view response = "One of ssid or password is missing";
        httpd_resp_send(req, response.data(), static_cast<ssize_t>(response.size()));
        return ESP_OK;
    }

    auto server = static_cast<TConfigServer*>(req->user_ctx);
    wifi_config_t config{};

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &CheckGotIp, static_cast<void*>(req->user_ctx)));
    std::copy(ssid->second.begin(), ssid->second.end(), config.ap.ssid);
    std::copy(password->second.begin(), password->second.end(), config.ap.password);
    ESP_LOGI(TAG, "Connecting to %s...", config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    auto result = server->WaitResult();
    esp_wifi_disconnect();
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &CheckGotIp);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    if (result) {
        if (!server->Setup(ssid->second, password->second)) {
            httpd_resp_set_status   (req, "500 Internal Server Error");
            httpd_resp_set_type     (req, HTTPD_TYPE_TEXT);
            std::string_view response = "Can't save data";
            httpd_resp_send(req, response.data(), static_cast<ssize_t>(response.size()));
            return ESP_OK;
        }
        std::string_view response (reinterpret_cast<const char *>(SuccessHtmlStart));
        httpd_resp_send(req, response.data(), static_cast<ssize_t>(response.size()));
        vTaskDelay(3000);
        esp_restart();
        return ESP_OK;
    } else {
        std::string_view response (reinterpret_cast<const char *>(LoginHtmlStart));
        std::string result = NInternal::ProcessTemplate(response, [server, ssid](std::string_view key)-> std::string {
            if (key == "SSID") {
                return NInternal::EncodeHtml(server->GetSsid());
            } else if (key == "MESSAGE") {
                return "<h3>Can't connect to Wi-Fi " + ssid->second + "</h3>";
            }
            return {};
        });
        httpd_resp_send(req, result.data(), static_cast<ssize_t>(response.size()));
        return ESP_OK;
    }
}

void TConfigServer::Start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&Handle_, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_uri_t config = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = ConfigGetHandler,
            .user_ctx  = this
        };
        httpd_register_uri_handler(Handle_, &config);
        httpd_uri_t hotspot = {
            .uri       = "/hotspot-detect.html",
            .method    = HTTP_GET,
            .handler   = ConfigGetHandler,
            .user_ctx  = this
        };
        httpd_register_uri_handler(Handle_, &hotspot);
        httpd_uri_t save = {
            .uri       = "/",
            .method    = HTTP_POST,
            .handler   = ConfigSaveHandler,
            .user_ctx  = this
        };
        httpd_register_uri_handler(Handle_, &save);
    } else {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

void TConfigServer::Stop() {
    httpd_stop(Handle_);
}