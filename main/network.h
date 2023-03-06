#pragma once

#include <memory>
#include <string_view>
#include <tcpip_adapter.h>

class INetworkCallback {
public:
    virtual ~INetworkCallback() = default;
    virtual void OnWifiDisconnected() = 0;
    virtual void OnWifiConnected(const tcpip_adapter_ip_info_t& info) = 0;
};

void StartSoftAP();
void ConnectWifi(std::string_view ssid, std::string_view password, INetworkCallback* callback);
