// The settings panel: F10 on the keyboard, or Back and Start together on a controller, opens and closes it while the
// game runs; Escape closes it too. Every row takes effect at once and is saved to settings.toml, apart from the few
// marked as needing a restart. Rows are stepped with the arrows beside the value, Left and Right on the keyboard or
// the d-pad; Up and Down move between rows.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX  // std::min and std::max, not windows.h's macros
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/app.h"
#include "gpu/ext.h"
#include "settings.h"

namespace {

std::atomic<bool> g_open{false};

// window sizes the primary monitor offers, widest last; {0, 0} first: the desktop's
const std::vector<app::Mode>& Resolutions() {
    static const std::vector<app::Mode> modes = [] {
        std::vector<app::Mode> out = {{0, 0}};
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++) {
            constexpr DWORD kMinHeight = 480;
            if (dm.dmPelsHeight < kMinHeight) continue;
            app::Mode m{uint32_t(dm.dmPelsWidth), uint32_t(dm.dmPelsHeight)};
            bool seen = std::any_of(out.begin() + 1, out.end(),
                                    [&](const app::Mode& o) { return o.width == m.width && o.height == m.height; });
            if (!seen) out.push_back(m);
        }
        std::sort(out.begin() + 1, out.end(), [](const app::Mode& a, const app::Mode& b) {
            return a.width != b.width ? a.width < b.width : a.height < b.height;
        });
        return out;
    }();
    return modes;
}

// the index of value in list, else of the nearest below it
template <typename T>
int IndexOf(const T* list, int count, T value) {
    int at = 0;
    for (int i = 0; i < count; i++)
        if (list[i] <= value) at = i;
    return at;
}

int Step(int index, int step, int count) { return std::clamp(index + step, 0, count - 1); }

// one row: the label, then "<  value  >"; returns -1, 0 or 1 for the step asked for
int Row(const char* label, const std::string& value, bool enabled = true) {
    float em = ImGui::GetFontSize();
    float width = ImGui::GetContentRegionAvail().x;
    float value_w = em * 12;
    float arrow_w = ImGui::GetFrameHeight();
    int step = 0;
    ImGui::PushID(label);
    ImGui::BeginDisabled(!enabled);
    float y = ImGui::GetCursorPosY();
    float x = ImGui::GetCursorPosX();
    // the selectable is what keyboard and controller navigation land on; Left and Right step it
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(width, ImGui::GetFrameHeight()));
    if (ImGui::IsItemFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) step = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) step = 1;
    }
    if (ImGui::IsItemClicked()) step = 1;
    ImGui::SetCursorPos(ImVec2(x + em * 0.5f, y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    float block = x + width - value_w - arrow_w * 2;
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav | ImGuiItemFlags_ButtonRepeat, true);
    ImGui::SetCursorPos(ImVec2(block, y));
    if (ImGui::ArrowButton("##dec", ImGuiDir_Left)) step = -1;
    float pad = (value_w - ImGui::CalcTextSize(value.c_str()).x) * 0.5f;
    ImGui::SetCursorPos(ImVec2(block + arrow_w + std::max(pad, 0.0f), y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(value.c_str());
    ImGui::SetCursorPos(ImVec2(block + arrow_w + value_w, y));
    if (ImGui::ArrowButton("##inc", ImGuiDir_Right)) step = 1;
    ImGui::PopItemFlag();
    ImGui::EndDisabled();
    ImGui::PopID();
    return enabled ? step : 0;
}

void Heading(const char* title) {
    ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * 0.4f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.30f, 1));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

const char* OnOff(bool on) { return on ? "ON" : "OFF"; }

// tenths of a percent as "4.5%"
std::string Tenths(uint32_t v) {
    char t[16];
    std::snprintf(t, sizeof(t), "%u.%u%%", v / 10, v % 10);
    return t;
}
// finer steps the lower a sensitivity is: 0.1% up to 10%, 0.5% up to 50%, 5% above
uint32_t FineStep(uint32_t v) { return v <= 100 ? 1 : v <= 500 ? 5 : 50; }

std::string Percent(uint32_t v) {
    char t[16];
    std::snprintf(t, sizeof(t), "%u%%", v);
    return t;
}

// a value stepped through a list of choices
template <typename T, size_t N>
bool StepList(const char* label, T& value, const T (&choices)[N], const char* const (&names)[N],
              bool enabled = true) {
    int at = IndexOf(choices, int(N), value);
    int step = Row(label, names[at], enabled);
    if (!step) return false;
    value = choices[Step(at, step, int(N))];
    return true;
}

// a number stepped by a fixed amount between limits
bool StepNumber(const char* label, uint32_t& value, uint32_t lo, uint32_t hi, uint32_t by, std::string text) {
    int step = Row(label, text);
    if (!step) return false;
    int64_t v = int64_t(value) + int64_t(step) * by;
    value = uint32_t(std::clamp<int64_t>(v, lo, hi));
    return true;
}

bool Flag(const char* label, bool& value, const char* note = nullptr) {
    std::string text = OnOff(value);
    if (note) text += note;
    if (!Row(label, text)) return false;
    value = !value;
    return true;
}

// a key binding: Enter, A or a click and the next key or mouse button pressed takes it
void KeyRow(int action) {
    bool waiting = game::Rebinding() == action;
    std::string text = waiting ? "PRESS A KEY  (ESC CANCELS)" : game::KeyName(game::g_keys[action]);
    float em = ImGui::GetFontSize();
    float width = ImGui::GetContentRegionAvail().x;
    ImGui::PushID(action + 1000);
    float y = ImGui::GetCursorPosY(), x = ImGui::GetCursorPosX();
    bool pressed = ImGui::Selectable("##key", waiting, ImGuiSelectableFlags_None, ImVec2(width, ImGui::GetFrameHeight()));
    if (pressed && game::Rebinding() < 0) game::StartRebind(action);
    ImGui::SetCursorPos(ImVec2(x + em * 0.5f, y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(game::kKeyActions[action].label);
    float tw = ImGui::CalcTextSize(text.c_str()).x;
    ImGui::SetCursorPos(ImVec2(x + width - em * 7 - tw * 0.5f, y));
    ImGui::AlignTextToFramePadding();
    if (waiting) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.30f, 1));
    ImGui::TextUnformatted(text.c_str());
    if (waiting) ImGui::PopStyleColor();
    ImGui::PopID();
}

void DrawRows() {
    app::Settings& s = app::Get();
    bool changed = false, display = false;

    Heading("DISPLAY");
    {
        static const app::DisplayMode modes[] = {app::DisplayMode::Fullscreen, app::DisplayMode::Borderless,
                                                 app::DisplayMode::Windowed};
        static const char* const names[] = {"FULLSCREEN", "BORDERLESS", "WINDOWED"};
        if (StepList("Display mode", s.display_mode, modes, names)) changed = display = true;
    }
    {
        const auto& res = Resolutions();
        int at = 0;
        for (size_t i = 0; i < res.size(); i++)
            if (res[i].width == s.width && res[i].height == s.height) at = int(i);
        char text[32] = "DESKTOP";
        if (at) std::snprintf(text, sizeof(text), "%u x %u", res[size_t(at)].width, res[size_t(at)].height);
        if (int step = Row("Resolution", text)) {
            at = Step(at, step, int(res.size()));
            s.width = res[size_t(at)].width;
            s.height = res[size_t(at)].height;
            changed = display = true;
        }
    }
    changed |= StepNumber("Render resolution", s.render_percent, 25, 200, 5, Percent(s.render_percent));
    changed |= Flag("Ultrawide", game::g_ultrawide);
    {
        static const uint32_t caps[] = {30, 60, 90, 120, 144, 165, 240, app::kFpsCapRefresh, 0};
        static const char* const names[] = {"30", "60", "90", "120", "144", "165", "240", "REFRESH RATE",
                                            "UNLIMITED"};
        // the list in display order, the stored values out of it: 0 and 1 sort before the rest
        int at = 0;
        for (int i = 0; i < int(std::size(caps)); i++)
            if (caps[i] == s.fps_cap) at = i;
        char custom[16];
        const char* text = names[at];
        bool listed = caps[at] == s.fps_cap;
        if (!listed) std::snprintf(custom, sizeof(custom), "%u", s.fps_cap), text = custom;
        if (int step = Row("Max frame rate", text)) {
            s.fps_cap = caps[Step(at, step, int(std::size(caps)))];
            changed = true;
        }
    }
    changed |= Flag("Vertical sync", s.vsync);
    changed |= Flag("Low latency", game::g_low_latency, "  (RESTART)");
    if (gpu::g_reflex_state.load() == 0) {
        Row("NVIDIA Reflex", "NOT AVAILABLE", false);
    } else {
        static const uint32_t modes[] = {0, 1, 2};
        static const char* const names[] = {"OFF", "ON", "ON + BOOST"};
        changed |= StepList("NVIDIA Reflex", game::g_reflex, modes, names);
    }
    changed |= Flag("FPS counter", game::g_show_fps);

    Heading("GRAPHICS");
    {
        static const uint32_t levels[] = {0, 2, 4, 8, 16};
        static const char* const names[] = {"GAME", "2X", "4X", "8X", "16X"};
        changed |= StepList("Anisotropic filtering", s.aniso, levels, names);
    }
    {
        static const uint32_t modes[] = {0, 1, 2};
        static const char* const names[] = {"OFF", "ON", "STRONG"};
        changed |= StepList("Ambient occlusion (SSAO)", game::g_ssao, modes, names);
    }
    changed |= Flag("Motion blur and haze (game's)", game::g_motion_blur);
    {
        char t[16];
        std::snprintf(t, sizeof(t), "%.1f M", game::g_ssao_radius / 100.0);
        changed |= StepNumber("Ambient occlusion reach", game::g_ssao_radius, 20, 500, 10, t);
    }
    changed |= StepNumber("Field of view", game::g_fov_percent, 50, 200, 5, Percent(game::g_fov_percent));
    changed |= StepNumber("Gun field of view", game::g_gun_fov_percent, 50, 300, 10,
                          Percent(game::g_gun_fov_percent));
    changed |= StepNumber("Crosshair and mission text size", game::g_hud_pixel_percent, 0, 600, 50,
                          game::g_hud_pixel_percent ? Percent(game::g_hud_pixel_percent) : std::string("GAME"));
    changed |= StepNumber("HUD size", game::g_hud_scale_percent, 25, 100, 5, Percent(game::g_hud_scale_percent));
    changed |= StepNumber("HUD out to the screen edges", game::g_hud_edge_percent, 0, 100, 10,
                          Percent(game::g_hud_edge_percent));

    Heading("CONTROLS");
    changed |= Flag("Call of Duty layout", game::g_cod_controls);
    changed |= StepNumber("Hipfire sensitivity", game::g_mouse_speed, 1, 5000, FineStep(game::g_mouse_speed),
                          Tenths(game::g_mouse_speed));
    changed |= StepNumber("ADS sensitivity", game::g_mouse_aim_speed, 1, 5000, FineStep(game::g_mouse_aim_speed),
                          Tenths(game::g_mouse_aim_speed));
    changed |= Flag("Invert mouse Y", game::g_mouse_invert);
    changed |= Flag("Weapon recoil", game::g_weapon_recoil);

    Heading("KEYBOARD AND MOUSE  (CLICK AN ACTION, THEN PRESS ITS KEY)");
    for (int i = 0; i < game::kActionCount; i++) KeyRow(i);
    if (Row("Reset keys to defaults", "RESET")) {
        game::DefaultKeys();
        changed = true;
    }

    Heading("GENERAL");
    changed |= Flag("Skip intros", game::g_skip_intro);
    changed |= Flag("Texture replacement", s.custom_textures);
    changed |= Flag("Texture dumping", s.dump_textures);

    if (game::TakeKeysChanged()) changed = true;
    if (changed) {
        game::ApplyGameSettings();
        if (display) app::ApplyDisplay();
        app::Save();
    }
}

void DrawPanel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float em = ImGui::GetFontSize();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(em * 40, std::min(em * 34, vp->Size.y * 0.9f)), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("##bf2mc_settings", nullptr, kFlags)) {
        ImGui::TextUnformatted("BATTLEFIELD 2: MODERN COMBAT  -  SETTINGS");
        float footer = ImGui::GetFrameHeightWithSpacing() + em;
        ImGui::BeginChild("##rows", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened);
        DrawRows();
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::TextDisabled("F10 / Back + Start / Esc to close   -   changes are saved at once");
        ImGui::SameLine(ImGui::GetContentRegionMax().x - em * 6);
        if (ImGui::Button("Close", ImVec2(em * 6, 0))) panel::Toggle();
    }
    ImGui::End();
}

void DrawFps(double fps) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 8, vp->Pos.y + 8));
    ImGui::SetNextWindowBgAlpha(0.45f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("##bf2mc_fps", nullptr, kFlags)) ImGui::Text("%.0f FPS", fps);
    ImGui::End();
}

}  // namespace

namespace panel {

bool Open() { return g_open.load(std::memory_order_relaxed); }

void Toggle() {
    if (game::Rebinding() >= 0) game::StartRebind(-1);
    g_open.store(!Open(), std::memory_order_relaxed);
}

bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    constexpr intptr_t kKeyWasDown = intptr_t(1) << 30;  // a held key's repeat
    game::MouseMessage(hwnd, msg, wp, lp);
    if (game::KeyMessage(hwnd, msg, wp, lp)) {
        *result = 0;
        return true;
    }
    if (msg == WM_SETCURSOR && game::MouseCaptured() && LOWORD(lp) == HTCLIENT) {
        SetCursor(nullptr);
        *result = TRUE;
        return true;
    }
    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wp == VK_F10 && !(lp & kKeyWasDown)) {
                Toggle();
                *result = 0;
                return true;
            }
            if (wp == VK_ESCAPE && Open()) {
                Toggle();
                *result = 0;
                return true;
            }
            break;
        case WM_SYSCOMMAND:
            // Alt let go alone would open the window menu, which holds the game: Alt is for binding
            if ((wp & 0xFFF0) == SC_KEYMENU && lp == 0) {
                *result = 0;
                return true;
            }
            break;
        case WM_SYSKEYUP:
            // F10 alone would open the window menu
            if (wp == VK_F10) {
                *result = 0;
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

void Draw(double fps) {
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = std::max(1.0f, io.DisplaySize.y / 720.0f);
    io.KeyRepeatRate = 0.1f;
    io.MouseDrawCursor = Open();
    // ImGui sets the Windows cursor on every WM_SETCURSOR unless told not to: shut, the game window's cursor is
    // mouse.cpp's (none while playing)
    if (Open()) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    if (game::g_show_fps) DrawFps(fps);
    if (Open()) DrawPanel();
}

}  // namespace panel
