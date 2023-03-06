#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <nvs.h>

class TStorage {
public:
    void Init(std::string_view name) {
        if (nvs_open(name.data(), NVS_READWRITE, &Handle_) != ESP_OK) {
            Handle_ = 0;
        }
    }

    [[nodiscard]] std::string Get(std::string_view name) const {
        size_t size = 0;
        nvs_get_blob(Handle_, name.data(), nullptr, &size);
        std::vector<char> result(size);
        nvs_get_blob(Handle_, name.data(), result.data(), &size);
        return {result.begin(), result.end()};
    }

    [[nodiscard]] bool Set(std::string_view name, std::string_view value) const {
        return nvs_set_blob(Handle_, name.data(), value.data(), value.size()) == ESP_OK;
    }

    void Erase() const {
        nvs_erase_all(Handle_);
    }

    [[nodiscard]] bool Commit() const {
        return nvs_commit(Handle_) == ESP_OK;
    }

private:
    nvs_handle Handle_ = 0;
};