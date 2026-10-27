#pragma once
#include <cstdint>
#include <string>

// the settings of this port, beside the runtime's (app::Settings): kept in settings.toml through GameInfo::options,
// changed in the F10 panel (panel.cpp)
namespace game {

extern uint32_t g_fov_percent;        // the view's field of view, 100 the game's (src/game/hooks.cpp)
extern uint32_t g_gun_fov_percent;    // the first person gun's field of view, 100 the game's
extern bool g_skip_intro;             // no EA, DICE, RenderWare and legal screens or intro movie at startup
extern bool g_cod_controls;           // controller: Call of Duty style layout in play, aim held on the left trigger
extern uint64_t g_camera_ms;          // when the game last set its play camera (src/game/hooks.cpp), 0 never
extern uint32_t g_hud_scale_percent;  // the rest of the play HUD, 100 the game's size
extern uint32_t g_hud_edge_percent;   // the HUD by the sides moved this far out to the window's edges
extern uint32_t g_hud_pixel_percent;  // magnified HUD art (crosshair, mission text): screen pixels a texel, 0 off
extern bool g_ultrawide;              // full window width past 16:9
extern bool g_low_latency;            // the swap chain queues one frame; read when the renderer starts
extern bool g_show_fps;
extern bool g_weapon_recoil;
extern uint32_t g_reflex;
extern bool g_motion_blur;            // the game's own motion blur and haze while playing
extern uint32_t g_ssao;               // ambient occlusion: 0 off, 1 on, 2 strong
extern uint32_t g_ssao_radius;        // its reach, hundredths of a world unit             // NVIDIA Reflex: 0 off, 1 on, 2 on + boost          // mouse aim: the game's kick up while firing comes through               // a frame rate counter in the corner

extern uint32_t g_mouse_sensitivity;      // percent
extern uint32_t g_mouse_aim_sensitivity;  // percent of that while aiming
extern uint32_t g_mouse_speed;            // tenths of a percent, these two take over from the percents
extern uint32_t g_mouse_aim_speed;
extern bool g_mouse_invert;

// keyboard and mouse buttons (keys.cpp): an action each, bound to a virtual key code, a mouse button or the wheel
enum {
    kMoveForward, kMoveBack, kMoveLeft, kMoveRight, kFire, kAim, kJump, kStance, kReload, kWeaponSelect,
    kHotswap, kVehicle, kParachute, kDpadUp, kDpadRight, kDpadDown, kDpadLeft, kPause, kMap, kActionCount
};
constexpr uint32_t kWheelUp = 0x100, kWheelDown = 0x101;
struct KeyAction {
    const char* key;    // settings.toml
    const char* label;  // the panel
    uint32_t default_key;
};
extern const KeyAction kKeyActions[kActionCount];
extern uint32_t g_keys[kActionCount];  // 0 unbound
void DefaultKeys();
bool KeyHeld(int action);              // the game window in front, the panel shut
void KeyWheel(int notches);            // from the raw mouse: up positive
void SelectMouse(long dx, long dy);    // the raw mouse while weapon select is held
std::string KeyName(uint32_t key);
void StartRebind(int action);          // the next key or mouse button pressed binds it; Escape cancels
int Rebinding();                       // the action being bound, -1 none
bool TakeKeysChanged();                // a binding changed since the last call
bool KeyMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp);  // true: taken for a rebind
}  // namespace game
namespace input {
struct Pad;
}
namespace game {
void ApplyKeys(input::Pad& pad, bool playing);  // the bound actions into the pad, after the controller layout

// the settings above handed to the renderer; at start and after each change in the panel
void ApplyGameSettings();

// the mouse (mouse.cpp): window messages, and while it is captured (playing, panel shut) its turn and buttons
void MouseMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp);
bool MouseCaptured();
bool MouseLeft();
bool MouseRight();
// radians the view turns since the last call, across and down; false for none
bool TakeMouse(float& yaw, float& pitch, bool aiming);

}  // namespace game

// the F10 panel
namespace panel {

bool Open();
void Toggle();
// GameInfo hooks: window messages (F10, Escape) and the ImGui frame
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
void Draw(double fps);

}  // namespace panel
