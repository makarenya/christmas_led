#pragma once

#include <cstddef>

enum class EOutputState {
    Unknown,
    On,
    Off,
    Toggle,
    Next,
    Static,
    Dynamic,
    Fireplace,
    Candle
};

class IOutputCallback {
public:
    virtual ~IOutputCallback() = default;
    virtual void OnOutputChanged(bool isOn, size_t mode) = 0;
};

void OutputSet(EOutputState command);
void OutputInit(IOutputCallback* callback);