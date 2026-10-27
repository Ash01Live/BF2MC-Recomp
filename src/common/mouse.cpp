// The mouse: raw counts (WM_INPUT, no Windows acceleration) turned into view angles the game's own way. The player's
// controls object carries the view's yaw and pitch in radians (channels 10 and 11, controls + 0x2C and + 0x30; found
// by src/game/hooks.cpp's look scan): src/game/hooks.cpp adds TakeMouse's turn to them each frame, so the mouse turns
// the view directly, with no stick dead zone, curve or turn speed cap.
// While playing, with the settings panel closed and the window in front, the cursor is hidden and kept in the
// window; the left button fires and the right one aims (src/common/game_info.cpp's pad hook).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>

#include "core/log.h"
#include "settings.h"

namespace game {

uint32_t g_mouse_sensitivity = 100;       // percent, older settings files; read into the tenths below
uint32_t g_mouse_aim_sensitivity = 70;
uint32_t g_mouse_speed = 0;               // tenths of a percent, 1000 = kRadiansPerCount; 0 unset
uint32_t g_mouse_aim_speed = 0;           // tenths of a percent while aiming down the sights, on its own
bool g_mouse_invert = false;              // pitch the other way

namespace {

constexpr float kRadiansPerCount = 0.0008f;  // at 100%: about 0.046 degrees a count
std::atomic<int64_t> g_dx{0}, g_dy{0};
std::atomic<bool> g_left{false}, g_right{false};
std::atomic<bool> g_captured{false};
bool g_registered = false;

uint64_t NowMs() {
    using namespace std::chrono;
    return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// the game's play camera was set within the last quarter second, the panel is shut and the window is in front
bool WantCapture(HWND hwnd) {
    constexpr uint64_t kPlayWindowMs = 250;
    uint64_t camera = g_camera_ms;
    bool playing = camera && NowMs() - camera < kPlayWindowMs;
    return playing && !panel::Open() && GetForegroundWindow() == hwnd;
}

void UpdateCapture(HWND hwnd) {
    bool want = WantCapture(hwnd);
    if (want) {
        RECT r;
        GetClientRect(hwnd, &r);
        POINT a{r.left, r.top}, b{r.right, r.bottom};
        ClientToScreen(hwnd, &a);
        ClientToScreen(hwnd, &b);
        RECT clip{a.x, a.y, b.x, b.y};
        ClipCursor(&clip);
        SetCursor(nullptr);
    } else if (g_captured.load()) {
        ClipCursor(nullptr);
        g_left = g_right = false;
        g_dx = g_dy = 0;
    }
    g_captured = want;
}

}  // namespace

bool MouseCaptured() { return g_captured.load(std::memory_order_relaxed); }
// fire and aim as bound (keys.cpp; the left and right buttons unless rebound), while playing
bool MouseLeft() { return MouseCaptured() && KeyHeld(kFire); }
bool MouseRight() { return MouseCaptured() && KeyHeld(kAim); }

bool TakeMouse(float& yaw, float& pitch, bool aiming) {
    int64_t dx = g_dx.exchange(0), dy = g_dy.exchange(0);
    if (!dx && !dy) return false;
    if (!MouseCaptured()) return false;
    // each its own: aiming does not scale the regular one
    float k = kRadiansPerCount * float(aiming ? g_mouse_aim_speed : g_mouse_speed) / 1000.0f;
    yaw = float(dx) * k;
    pitch = float(dy) * k * (g_mouse_invert ? -1.0f : 1.0f);
    return true;
}

// window messages, before the panel's: raw mouse input and the cursor
void MouseMessage(void* hwnd_, uint32_t msg, uintptr_t wp, intptr_t lp) {
    HWND hwnd = static_cast<HWND>(hwnd_);
    if (!g_registered) {
        RAWINPUTDEVICE rid{0x01, 0x02, 0, hwnd};  // generic desktop, mouse
        g_registered = RegisterRawInputDevices(&rid, 1, sizeof(rid));
        LOGI("mouse: raw input %s", g_registered ? "on" : "unavailable");
    }
    switch (msg) {
        case WM_INPUT: {
            RAWINPUT ri;
            UINT size = sizeof(ri);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) ==
                    UINT(-1) ||
                ri.header.dwType != RIM_TYPEMOUSE)
                break;
            const RAWMOUSE& m = ri.data.mouse;
            UpdateCapture(hwnd);
            if (!MouseCaptured()) break;
            if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                // holding weapon select the mouse points the game's weapon pick (keys.cpp), the view stays
                if (KeyHeld(kWeaponSelect)) SelectMouse(m.lLastX, m.lLastY);
                else g_dx += m.lLastX, g_dy += m.lLastY;
            }
            USHORT f = m.usButtonFlags;
            if (f & RI_MOUSE_LEFT_BUTTON_DOWN) g_left = true;
            if (f & RI_MOUSE_LEFT_BUTTON_UP) g_left = false;
            if (f & RI_MOUSE_RIGHT_BUTTON_DOWN) g_right = true;
            if (f & RI_MOUSE_RIGHT_BUTTON_UP) g_right = false;
            if (f & RI_MOUSE_WHEEL) {
                int delta = SHORT(ri.data.mouse.usButtonData), notches = delta / WHEEL_DELTA;
                if (!notches && delta) notches = delta > 0 ? 1 : -1;
                if (MouseCaptured()) KeyWheel(notches);
            }
            break;
        }
        case WM_ACTIVATE:
        case WM_SIZE:
        case WM_MOVE:
        case WM_TIMER:
            UpdateCapture(hwnd);
            break;
        default:
            break;
    }
    (void)wp;
}

}  // namespace game
