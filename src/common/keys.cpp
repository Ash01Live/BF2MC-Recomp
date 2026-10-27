// Keyboard and mouse buttons: each of the game's actions bound to a key or a mouse button, rebound in the F10 panel
// (CONTROLS) and kept in settings.toml as key_<action> (a Windows virtual key code; 1 2 4 5 6 the mouse buttons,
// 0x100 and 0x101 the wheel up and down, 0 unbound). While playing, a held binding presses the game's own button for
// that action, whatever the controller layout (game_info.cpp's PadRemap applies them after its remap); movement keys
// push the left stick fully. In the menus the arrows, Enter and Escape move, choose and go back.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <string>

#include "core/input.h"
#include "settings.h"

namespace game {

const KeyAction kKeyActions[kActionCount] = {
    {"key_forward", "Move forward", 'W'},
    {"key_back", "Move back", 'S'},
    {"key_left", "Move left", 'A'},
    {"key_right", "Move right", 'D'},
    {"key_fire", "Fire", VK_LBUTTON},
    {"key_aim", "Aim down sights", VK_RBUTTON},
    {"key_jump", "Jump", VK_SPACE},
    {"key_stance", "Crouch / prone", 'C'},
    {"key_reload", "Reload", 'R'},
    {"key_weapon_select", "Weapon select (hold, point with the mouse)", VK_MENU},
    {"key_hotswap", "Hotswap", 'F'},
    {"key_vehicle", "Enter / leave vehicle", 'E'},
    {"key_parachute", "Parachute", 'V'},
    {"key_dpad_up", "D-pad up", '1'},
    {"key_dpad_right", "D-pad right", '2'},
    {"key_dpad_down", "D-pad down", '3'},
    {"key_dpad_left", "D-pad left", '4'},
    {"key_pause", "Pause", VK_ESCAPE},
    {"key_map", "Map / scores (Back)", VK_TAB},
};

// the defaults until settings.toml says otherwise, in kKeyActions' order
uint32_t g_keys[kActionCount] = {'W', 'S', 'A', 'D', VK_LBUTTON, VK_RBUTTON, VK_SPACE, 'C', 'R',
                                 VK_MENU, 'F', 'E', 'V', '1', '2', '3', '4', VK_ESCAPE, VK_TAB};

}  // namespace game

namespace {

std::atomic<HWND> g_hwnd{nullptr};
std::atomic<int> g_wheel_up{0}, g_wheel_down{0};
std::atomic<int> g_rebinding{-1};
std::atomic<bool> g_keys_changed{false};

uint64_t NowMs() {
    using namespace std::chrono;
    return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

bool Foreground() {
    HWND h = g_hwnd.load();
    return h && GetForegroundWindow() == h;
}

bool Down(uint32_t vk) { return vk && vk < 0x100 && (GetAsyncKeyState(int(vk)) & 0x8000); }

// a wheel notch as a press of 50 ms, then 50 ms up so the next notch is a new press
struct Pulse {
    uint64_t until = 0, next = 0;
    bool Held(std::atomic<int>& notches, uint64_t now) {
        if (now >= next && notches.load() > 0) {
            notches.fetch_sub(1);
            until = now + 50, next = now + 100;
        }
        return now < until;
    }
};
Pulse g_pulse[2];

// Weapon select: the game picks a weapon while its weapon button (the left trigger) is held, by the right stick's
// direction. Held on the keyboard, the mouse points instead: its movement, kept within twice kPickCounts and
// springing back to the middle, pushes the stick fully that way once past kPickCounts (ApplyKeys).
constexpr float kPickCounts = 40;
std::atomic<long> g_pick_x{0}, g_pick_y{0};

}  // namespace

namespace game {

void DefaultKeys() {
    for (int i = 0; i < kActionCount; i++) g_keys[i] = kKeyActions[i].default_key;
}

bool KeyHeld(int action) {
    if (action < 0 || action >= kActionCount || !Foreground() || panel::Open()) return false;
    uint32_t k = g_keys[action];
    uint64_t now = NowMs();
    if (k == kWheelUp) return g_pulse[0].Held(g_wheel_up, now);
    if (k == kWheelDown) return g_pulse[1].Held(g_wheel_down, now);
    return Down(k);
}

void SelectMouse(long dx, long dy) {
    float x = float(g_pick_x.load() + dx), y = float(g_pick_y.load() + dy);
    float len = std::sqrt(x * x + y * y), cap = kPickCounts * 2;
    if (len > cap) x *= cap / len, y *= cap / len;
    g_pick_x.store(long(x));
    g_pick_y.store(long(y));
}

void KeyWheel(int notches) {
    if (notches > 0) g_wheel_up.fetch_add(notches);
    if (notches < 0) g_wheel_down.fetch_add(-notches);
}

std::string KeyName(uint32_t k) {
    switch (k) {
        case 0: return "-";
        case VK_LBUTTON: return "LEFT MOUSE";
        case VK_RBUTTON: return "RIGHT MOUSE";
        case VK_MBUTTON: return "MIDDLE MOUSE";
        case VK_XBUTTON1: return "MOUSE 4";
        case VK_XBUTTON2: return "MOUSE 5";
        case kWheelUp: return "WHEEL UP";
        case kWheelDown: return "WHEEL DOWN";
        case VK_SHIFT: return "SHIFT";
        case VK_CONTROL: return "CTRL";
        case VK_MENU: return "ALT";
        case VK_SPACE: return "SPACE";
        case VK_ESCAPE: return "ESC";
        case VK_TAB: return "TAB";
        case VK_RETURN: return "ENTER";
        case VK_BACK: return "BACKSPACE";
        case VK_CAPITAL: return "CAPS LOCK";
        case VK_UP: return "UP";
        case VK_DOWN: return "DOWN";
        case VK_LEFT: return "LEFT";
        case VK_RIGHT: return "RIGHT";
        default: break;
    }
    if ((k >= '0' && k <= '9') || (k >= 'A' && k <= 'Z')) return std::string(1, char(k));
    if (k >= VK_F1 && k <= VK_F24) return "F" + std::to_string(k - VK_F1 + 1);
    if (k >= VK_NUMPAD0 && k <= VK_NUMPAD9) return "NUM " + std::to_string(k - VK_NUMPAD0);
    wchar_t w[64] = {};
    LONG scan = LONG(MapVirtualKeyW(k, MAPVK_VK_TO_VSC)) << 16;
    if (scan && GetKeyNameTextW(scan, w, 64) > 0) {
        char out[64];
        int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, sizeof(out), nullptr, nullptr);
        if (n > 0) {
            std::string s(out);
            for (char& c : s) c = char(toupper(static_cast<unsigned char>(c)));
            return s;
        }
    }
    char t[16];
    std::snprintf(t, sizeof(t), "KEY %u", k);
    return t;
}

void StartRebind(int action) { g_rebinding.store(action); }
bool TakeKeysChanged() { return g_keys_changed.exchange(false); }
int Rebinding() { return g_rebinding.load(); }

// the binding being changed takes this key; no other action keeps it
static void Bind(uint32_t k) {
    int a = g_rebinding.exchange(-1);
    if (a < 0 || a >= kActionCount) return;
    for (uint32_t& other : g_keys)
        if (other == k) other = 0;
    g_keys[a] = k;
    g_keys_changed.store(true);
}

bool KeyMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp) {
    g_hwnd.store(static_cast<HWND>(hwnd));
    if (g_rebinding.load() < 0) return false;
    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wp == VK_ESCAPE) {
                g_rebinding.store(-1);  // cancelled
                return true;
            }
            if (wp == VK_F10) return false;
            Bind(uint32_t(wp));
            return true;
        case WM_LBUTTONDOWN: Bind(VK_LBUTTON); return true;
        case WM_RBUTTONDOWN: Bind(VK_RBUTTON); return true;
        case WM_MBUTTONDOWN: Bind(VK_MBUTTON); return true;
        case WM_XBUTTONDOWN: Bind(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2); return true;
        case WM_MOUSEWHEEL: Bind(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? kWheelUp : kWheelDown); return true;
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        case WM_CHAR:
            return true;
        default:
            break;
    }
    (void)lp;
    return false;
}

// the bound actions into the pad the game gets, in the game's own buttons
void ApplyKeys(input::Pad& p, bool playing) {
    if (!Foreground() || panel::Open() || g_rebinding.load() >= 0) return;
    enum { kA, kB, kX, kY, kBlack, kWhite, kLeftTrigger, kRightTrigger };
    constexpr uint16_t kUp = 0x1, kDown = 0x2, kLeft = 0x4, kRight = 0x8, kStart = 0x10, kBack = 0x20;
    // the menus, the pause menu among them: the arrows move, Enter chooses; out of play Escape and Backspace go back
    if (Down(VK_UP)) p.buttons |= kUp;
    if (Down(VK_DOWN)) p.buttons |= kDown;
    if (Down(VK_LEFT)) p.buttons |= kLeft;
    if (Down(VK_RIGHT)) p.buttons |= kRight;
    if (Down(VK_RETURN)) p.analog[kA] = 0xFF;
    if (!playing) {
        if (Down(VK_ESCAPE) || Down(VK_BACK)) p.analog[kB] = 0xFF;
        return;
    }
    int x = int(KeyHeld(kMoveRight)) - int(KeyHeld(kMoveLeft));
    int y = int(KeyHeld(kMoveForward)) - int(KeyHeld(kMoveBack));
    if (x) p.lx = int16_t(x * 32767);
    if (y) p.ly = int16_t(y * 32767);
    auto press = [&](int action, int analog) {
        if (KeyHeld(action)) p.analog[analog] = 0xFF;
    };
    press(kFire, kRightTrigger);
    press(kJump, kA);
    press(kVehicle, kB);
    press(kStance, kX);
    press(kHotswap, kY);
    press(kParachute, kBlack);
    press(kReload, kWhite);
    press(kWeaponSelect, kLeftTrigger);
    // the mouse pointing the weapon pick while weapon select is held: a stick that springs back to the middle, so
    // flicks one after another each pass through it the way a thumb on the stick does; a flick to another side gets
    // a moment in the middle first (the game holds its pick until the stick has been back)
    static uint64_t last = 0, middle_until = 0;
    static float side_x = 0, side_y = 0;  // the side pushed last, unit, 0 0 none
    uint64_t now = NowMs();
    float dt = last ? float(std::min<uint64_t>(now - last, 100)) : 0;
    last = now;
    if (KeyHeld(kWeaponSelect)) {
        float x = float(g_pick_x.load()), y = float(g_pick_y.load());
        float spring = std::exp(-dt / 90.0f);  // back to the middle within a fifth of a second
        x *= spring, y *= spring;
        g_pick_x.store(long(x));
        g_pick_y.store(long(y));
        float len = std::sqrt(x * x + y * y);
        if (len > kPickCounts) {
            float ux = x / len, uy = y / len;
            bool other = (side_x || side_y) && ux * side_x + uy * side_y < 0.5f;  // more than 60 degrees round
            if (other) middle_until = now + 40;
            side_x = ux, side_y = uy;
            if (now >= middle_until) {
                p.rx = int16_t(ux * 32767);
                p.ry = int16_t(-uy * 32767);  // the mouse's down, the stick's up
            }
        }
    } else {
        g_pick_x.store(0);
        g_pick_y.store(0);
        side_x = side_y = 0;
    }
    auto button = [&](int action, uint16_t bit) {
        if (KeyHeld(action)) p.buttons |= bit;
    };
    button(kDpadUp, kUp);
    button(kDpadRight, kRight);
    button(kDpadDown, kDown);
    button(kDpadLeft, kLeft);
    button(kPause, kStart);
    button(kMap, kBack);
}

}  // namespace game
