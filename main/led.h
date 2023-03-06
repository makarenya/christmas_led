#pragma once
#include "hardware.h"

enum class ELedState {
        On,
        Off,
        Sta,
        Static,
        Dynamic,
        Fireplace,
        Candle,
};

void LedInit();
void LedSet(ELedState state);