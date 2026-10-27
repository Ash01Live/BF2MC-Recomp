#include <algorithm>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"

#include <chrono>
#include "settings.h"
#include "gpu/ext.h"

namespace game {
uint32_t g_gun_fov_percent = 100;
uint32_t g_fov_percent = 100;
bool g_skip_intro = true;
bool g_cod_controls = true;
uint64_t g_camera_ms = 0;
uint32_t g_hud_pixel_percent = 100;
uint32_t g_hud_scale_percent = 50;   // the play HUD's pieces, patches/sdk-zz-hud-layout.patch
uint32_t g_hud_edge_percent = 100;   // of the way out to the window's edges
bool g_ultrawide = true;
bool g_low_latency = true;  // patches/sdk-z-frame-latency.patch
bool g_show_fps = false;
bool g_weapon_recoil = true;       // the game's kick up while firing the mouse (src/game/hooks.cpp)
uint32_t g_reflex = 1;             // NVIDIA Reflex: 0 off, 1 on, 2 on + boost (patches/sdk-zzz-reflex.patch)
bool g_motion_blur = true;         // the game's own blur and haze (patches/sdk-zzzz-ssao.patch)
uint32_t g_ssao = 1;               // 0 off, 1 on, 2 strong (patches/sdk-zzzz-ssao.patch)
uint32_t g_ssao_radius = 100;      // the occlusion's reach, hundredths of a world unit

void ApplyGameSettings() {
    // Ultrawide: a window wider than 16:9 shows the game at its full width, the 3D scene widened and the HUD kept a
    // 16:9 block in the middle (patches/sdk-ultrawide.patch). ultrawide = false keeps 16:9 with bars.
    gpu::g_ultrawide = g_ultrawide;
    // the gun's projection rows, scaled per draw by the renderer (patches/sdk-ultrawide.patch): 180 shows it 1.8
    // times as wide, as a zoom out
    uint32_t gun = std::clamp<uint32_t>(g_gun_fov_percent, 50, 300);
    gpu::g_gun_zoom = 100.0f / float(gun);
    // the crosshair and the mission text at their own pixel size: 100 one screen pixel a texel, 200 two, 0 the
    // title's size (patches/sdk-widescreen-hud.patch)
    gpu::g_hud_native = float(std::min<uint32_t>(g_hud_pixel_percent, 1000)) / 100.0f;
    // the rest of the play HUD: its pieces this size, those by the sides moved this far out to the window's edges
    gpu::g_hud_scale = float(std::clamp<uint32_t>(g_hud_scale_percent, 10, 200)) / 100.0f;
    gpu::g_hud_edge = float(std::min<uint32_t>(g_hud_edge_percent, 100)) / 100.0f;
    gpu::g_reflex.store(int(std::min<uint32_t>(g_reflex, 2)));
    gpu::g_ssao.store(int(std::min<uint32_t>(g_ssao, 2)));
    gpu::g_game_blur.store(g_motion_blur);
    gpu::g_ssao_radius = float(std::clamp<uint32_t>(g_ssao_radius, 10, 1000)) / 100.0f;
}
}  // namespace game

namespace {

uint64_t NowMs() {
    using namespace std::chrono;
    return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// Call of Duty style controls while playing (the play camera set within the last quarter second; the menus keep the
// game's buttons, B going back). The game's own layout, per button the game sees:
//   A jump, B enter or leave a vehicle, X stand / crouch / prone, Y hotswap, white reload, black parachute,
//   left trigger the next weapon (held with the d-pad: pick one), right trigger fire, right stick click zoom (a toggle)
// remapped to:
//   left trigger: zoom while held (a click as it is pulled and another as it is let go)
//   B stance, X reload, Y next weapon, LB (white) enter or leave a vehicle, right stick click hotswap; A, RB (black),
//   the right trigger, the sticks and the d-pad as they were
void PadLayout(int port, input::Pad& p);

// the controller layout, then the keyboard's and mouse's bound actions in the game's own buttons
void PadRemap(int port, input::Pad& p) {
    PadLayout(port, p);
    if (port != 0 || panel::Open()) return;
    bool playing = game::g_camera_ms && NowMs() - game::g_camera_ms < 250;
    game::ApplyKeys(p, playing);
}

void PadLayout(int port, input::Pad& p) {
    constexpr uint16_t kRightThumb = 0x80;
    constexpr uint64_t kPlayWindowMs = 250;  // play camera set this recently: playing
    constexpr uint64_t kClickMs = 60;         // a zoom click: held this long, then released at least as long
    constexpr uint8_t kPullOn = 40, kPullOff = 25;
    enum { kA, kB, kX, kY, kBlack, kWhite, kLeftTrigger, kRightTrigger };
    struct State {
        bool playing = false, held = false, zoomed = false;
        uint64_t click_until = 0, next_click = 0;
    };
    static State states[4];
    // the keyboard and mouse play as the first pad, a controller there or not
    if (port == 0) p.connected = true;
    if (port < 0 || port > 3 || !p.connected) return;
    // Back and Start together open or close the F10 panel; while it is open the game gets an idle pad
    constexpr uint16_t kStart = 0x10, kBack = 0x20;
    static bool combo[4];
    bool both = (p.buttons & (kStart | kBack)) == (kStart | kBack);
    if (both && !combo[port]) panel::Toggle();
    combo[port] = both;
    if (panel::Open() || both) {
        p = input::Pad{.connected = true};
        states[port] = {};
        return;
    }
    // the mouse's buttons on the first pad: left fires, right aims (held, the left trigger of the Call of Duty layout)
    static bool right_was[4];
    bool mouse_right = port == 0 && game::MouseRight();
    if (!game::g_cod_controls) {
        if (mouse_right != right_was[port]) p.buttons |= kRightThumb;  // the game's zoom toggle, each press and release
        right_was[port] = mouse_right;
        return;
    }
    if (mouse_right) p.analog[kLeftTrigger] = 0xFF;
    State& st = states[port];
    uint64_t now = NowMs();
    bool playing = game::g_camera_ms && now - game::g_camera_ms < kPlayWindowMs;
    if (playing != st.playing) {
        st = {playing};
        LOGI("controls: %s layout", playing ? "play (Call of Duty)" : "menu (the game's)");
    }
    if (!playing) return;
    input::Pad in = p;
    p.analog[kB] = in.analog[kWhite];
    p.analog[kX] = in.analog[kB];
    p.analog[kWhite] = in.analog[kX];
    p.analog[kLeftTrigger] = in.analog[kY];
    p.analog[kY] = (in.buttons & kRightThumb) ? 0xFF : 0;
    p.buttons &= uint16_t(~kRightThumb);
    // zoom held: the game's toggle clicked whenever the trigger and the zoom part ways
    uint8_t pull = in.analog[kLeftTrigger];
    st.held = st.held ? pull > kPullOff : pull > kPullOn;
    if (now >= st.click_until && st.held != st.zoomed && now >= st.next_click) {
        st.zoomed = st.held;
        st.click_until = now + kClickMs;
        st.next_click = now + 2 * kClickMs;
    }
    if (now < st.click_until) p.buttons |= kRightThumb;
}

// after the settings load, before the window and the renderer
void Init() {
    // fine sensitivities, from the older percents the first time
    if (!game::g_mouse_speed) game::g_mouse_speed = game::g_mouse_sensitivity * 10;
    if (!game::g_mouse_aim_speed) game::g_mouse_aim_speed = game::g_mouse_aim_sensitivity * 10;
    // RenderWare's 2D (menus, loading screens) draws at z = -0.1 in screen space with depth test ALWAYS. The NV2A
    // clamps that depth; D3D12's default near plane clipping dropped every such draw, leaving the menus black.
    // Needs patches/sdk-depth-clamp.patch.
    gpu::g_depth_clamp = true;
    gpu::g_low_latency = game::g_low_latency;
    game::ApplyGameSettings();
    // the settings file is written back with every key, so the game's own are there to edit
    app::Save();
}

}  // namespace

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Battlefield 2: Modern Combat",  // window and splash
    .short_title = L"BF2:MC",                  // message boxes
    .name = L"BF2MC",                          // window classes, crash dumps, handover event
    .log = "bf2mc",                            // log file prefix
    .settings = "settings.toml",               // settings file beside the exe
    .splash = L"splash",                       // optional splash.png or splash.bmp beside the exe
    .missing = L"Put Battlefield 2: Modern Combat (USA) in the assets folder: its disc image (.iso), or default.xbe "
               L"with the .viv files and folders beside it.",
    .images = {{"default.xbe", "BF2MC"}},      // each recompiled xbe and its exe, for title launches
    .options = {{"ultrawide", &game::g_ultrawide, nullptr},           // full window width past 16:9
                {"fov_percent", nullptr, &game::g_fov_percent},           // view width, 100 the game's
                {"gun_fov_percent", nullptr, &game::g_gun_fov_percent},   // gun width, 100 the game's
                {"skip_intro", &game::g_skip_intro, nullptr},                  // straight to the menu
                {"hud_pixel_percent", nullptr, &game::g_hud_pixel_percent},   // crosshair, mission text size
                {"hud_scale_percent", nullptr, &game::g_hud_scale_percent},   // the rest of the play HUD's size
                {"hud_edge_percent", nullptr, &game::g_hud_edge_percent},     // the HUD out to the window's edges
                {"cod_controls", &game::g_cod_controls, nullptr},              // Call of Duty controller layout
                {"low_latency", &game::g_low_latency, nullptr},
                {"reflex", nullptr, &game::g_reflex},
                {"ssao", nullptr, &game::g_ssao},
                {"motion_blur", &game::g_motion_blur, nullptr},                // the game's blur and haze                              // 0 off, 1 on, 2 strong
                {"ssao_radius", nullptr, &game::g_ssao_radius},                // hundredths of a world unit                          // 0 off, 1 on, 2 on + boost               // one frame queued for the display
                {"show_fps", &game::g_show_fps, nullptr},                      // frame rate counter
                {"mouse_sensitivity", nullptr, &game::g_mouse_sensitivity},    // percent
                {"mouse_aim_sensitivity", nullptr, &game::g_mouse_aim_sensitivity},  // percent, aiming
                {"mouse_speed", nullptr, &game::g_mouse_speed},                // tenths of a percent
                {"mouse_aim_speed", nullptr, &game::g_mouse_aim_speed},        // tenths, aiming
                {"mouse_invert", &game::g_mouse_invert, nullptr},
                {"weapon_recoil", &game::g_weapon_recoil, nullptr},          // the kick up while firing             // mouse pitch the other way
                {"key_forward", nullptr, &game::g_keys[game::kMoveForward]},  // keyboard and mouse bindings
                {"key_back", nullptr, &game::g_keys[game::kMoveBack]},
                {"key_left", nullptr, &game::g_keys[game::kMoveLeft]},
                {"key_right", nullptr, &game::g_keys[game::kMoveRight]},
                {"key_fire", nullptr, &game::g_keys[game::kFire]},
                {"key_aim", nullptr, &game::g_keys[game::kAim]},
                {"key_jump", nullptr, &game::g_keys[game::kJump]},
                {"key_stance", nullptr, &game::g_keys[game::kStance]},
                {"key_reload", nullptr, &game::g_keys[game::kReload]},
                {"key_weapon_select", nullptr, &game::g_keys[game::kWeaponSelect]},
                {"key_hotswap", nullptr, &game::g_keys[game::kHotswap]},
                {"key_vehicle", nullptr, &game::g_keys[game::kVehicle]},
                {"key_parachute", nullptr, &game::g_keys[game::kParachute]},
                {"key_dpad_up", nullptr, &game::g_keys[game::kDpadUp]},
                {"key_dpad_right", nullptr, &game::g_keys[game::kDpadRight]},
                {"key_dpad_down", nullptr, &game::g_keys[game::kDpadDown]},
                {"key_dpad_left", nullptr, &game::g_keys[game::kDpadLeft]},
                {"key_pause", nullptr, &game::g_keys[game::kPause]},
                {"key_map", nullptr, &game::g_keys[game::kMap]}},
    .load = nullptr,                           // reads more from the settings file's toml::table
    .save = nullptr,                           // writes more into it
    .init = Init,                              // after the settings load, before the window and boot
    .loaded = nullptr,                         // image in memory, before its entry point runs
    .window = nullptr,                         // the game window is up
    .message = panel::Message,                 // window messages first; true takes one
    .ui = panel::Draw,                         // ImGui drawing each frame
    .pad = PadRemap,                           // changes a polled pad
    .rumble = nullptr,                         // changes a rumble before it is set
};
