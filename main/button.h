#pragma once

class IButtonCallback {
public:
    virtual ~IButtonCallback() = default;
    virtual void OnButtonToggle() = 0;
    virtual void OnButtonNext() = 0;
    virtual void OnButtonResetWindowBegin() = 0;
    virtual void OnButtonResetWindowEnd() = 0;
    virtual void OnButtonReset() = 0;
};

void ButtonInit(IButtonCallback* callback);