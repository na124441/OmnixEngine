#pragma once

#include "InputDevice.h"
#include "InputEvent.h"
#include <cmath>
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class MouseInput : public InputDevice {
private:
    float posX = 0.0f;
    float posY = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    bool buttonCurrent[3] = {false, false, false};
    bool buttonPrevious[3] = {false, false, false};
    float scrollDelta = 0.0f;
    float prevPosX = 0.0f;
    float prevPosY = 0.0f;
    bool m_FirstUpdate = true;

public:
    MouseInput(uint32_t id)
        : InputDevice(id, DeviceType::Mouse) {}

    void SetPosition(float x, float y) {
        if (m_FirstUpdate) {
            prevPosX = x;
            prevPosY = y;
            m_FirstUpdate = false;
        }
        posX = x;
        posY = y;
    }

    void SetButton(int button, bool down) {
        if (button >= 0 && button < 3) {
            buttonCurrent[button] = down;
        }
    }

    void AddScroll(float delta) {
        scrollDelta += delta;
    }

    std::pair<float, float> GetMousePosition() const { return {posX, posY}; }
    std::pair<float, float> GetMouseDelta() const { return {deltaX, deltaY}; }
    float GetDeltaX() const { return deltaX; }
    float GetDeltaY() const { return deltaY; }
    bool IsMouseButtonDown(int button) const {
        if (button >= 0 && button < 3) return buttonCurrent[button];
        return false;
    }

    void OnCursorPos(double x, double y) {
        SetPosition(static_cast<float>(x), static_cast<float>(y));
    }

    void OnMouseButton(int button, int action) {
        SetButton(button, action != 0);
    }

    void OnScroll(double xoffset, double yoffset) {
        AddScroll(static_cast<float>(yoffset));
    }

    void UpdateState() override {
#ifdef _WIN32
        POINT pt;
        if (GetCursorPos(&pt)) {
            HWND activeWnd = GetActiveWindow();
            if (activeWnd) {
                ScreenToClient(activeWnd, &pt);
            }
            if (m_FirstUpdate) {
                prevPosX = static_cast<float>(pt.x);
                prevPosY = static_cast<float>(pt.y);
                m_FirstUpdate = false;
            }
            posX = static_cast<float>(pt.x);
            posY = static_cast<float>(pt.y);
        }
        buttonCurrent[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        buttonCurrent[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        buttonCurrent[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
#endif
    }

    void GenerateEvents(std::vector<InputEvent>& outEvents) override {
        deltaX = posX - prevPosX;
        deltaY = posY - prevPosY;

        if (std::fabs(deltaX) > 0.001f || std::fabs(deltaY) > 0.001f) {
            outEvents.push_back(
                InputEvent::CreateMouseMoveEvent(deviceID, deltaX, deltaY, posX, posY)
            );
        }

        for (int i = 0; i < 3; ++i) {
            if (!buttonPrevious[i] && buttonCurrent[i]) {
                outEvents.push_back(InputEvent::CreateMouseButtonDownEvent(deviceID, i));
            }
            else if (buttonPrevious[i] && !buttonCurrent[i]) {
                outEvents.push_back(InputEvent::CreateMouseButtonUpEvent(deviceID, i));
            }
            buttonPrevious[i] = buttonCurrent[i];
        }

        if (std::fabs(scrollDelta) > 0.001f) {
            outEvents.push_back(InputEvent::CreateMouseScrollEvent(deviceID, scrollDelta));
            scrollDelta = 0.0f;
        }

        prevPosX = posX;
        prevPosY = posY;
    }

    std::string GetDeviceName() const override { return "Mouse"; }
};

