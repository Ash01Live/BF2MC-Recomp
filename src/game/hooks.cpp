// Hooks for default.xbe. A strong definition replaces the recompiled function at that address; __imp__sub_X reaches
// the original, for example:
//
// X86_EXTERN_FUNC(__imp__sub_00012345);
// X86_FUNC(sub_00012345) {
//     __imp__sub_00012345(ctx, base);  // the original
// }

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <set>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "gpu/ext.h"
#include "xdk/d3d8.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// FATX cluster size of the Xbox hard disk's partitions: 16 KiB
constexpr uint32_t kFatxClusterSize = 0x4000;

}  // namespace

// XAPI's cluster size of the volume a file handle is on (stdcall, one argument: the handle). It references the
// handle's file object, asks IoQueryVolumeInformation for the file system's name, and for "FATX" reads the cluster
// size from FileObject->DeviceObject's volume extension (+0x1C). The runtime's file objects have no Xbox device
// object behind them, so the read went to address 0x0000000A. Every volume the game sees is a FATX partition with
// 16 KiB clusters, which is the answer the original gives on a console.
X86_FUNC(sub_000160C8) { Return(ctx, kFatxClusterSize, 1); }

// The game's own frame buffers and depth surface, which it hands to Direct3D_CreateDevice, are attached to the device
// by patches/sdk-frame-buffers.patch.

// The view's field of view, from the settings file's fov_percent (src/common/game_info.cpp).
// 0x00152840 hands the render camera its fov each frame (camera->vtable[4](fov)): the camera manager's
// ([0x451498]+0x30) plus an offset ([0x451444]). The setting widens it as a zoom would, in tangent space, so 100 is
// the game's own view and every other value keeps the picture undistorted. The gun has a projection of its own.
namespace game {
extern uint32_t g_fov_percent;
extern uint64_t g_camera_ms;
}

namespace {
float g_view_fov = 0;  // the game's own fov for the view this frame, before the settings widen it
}

// zoomed in: the game narrows the view's fov for the sights and the scope, whether held or toggled
// Zoomed in: a play camera's fov (each camera's projection and frustum, radians) narrowed below 85% of the
// widest it has had, on a camera still in use this last tenth of a second. The view's base fov (CameraFov) settles
// once on spawning, so it can't tell; the cameras' own fovs are what the sights and the scope narrow.
struct CamFov {
    uint32_t cam = 0;
    float widest = 0, now = 0, logged = 0;
    double seen = 0;
};
CamFov g_cam_fov[16];

double NowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void NoteCamFov(uint32_t cam, float fov, const char* where) {
    if (!(fov > 0.05f && fov < 3.1f)) return;
    CamFov* c = nullptr;
    for (auto& e : g_cam_fov)
        if (e.cam == cam || !e.cam) {
            c = &e;
            break;
        }
    if (!c) return;
    c->cam = cam;
    c->now = fov;
    c->widest = std::max(c->widest, fov);
    c->seen = NowSeconds();
    static int lines = 0;
    if (lines < 80 && std::fabs(fov - c->logged) > c->logged * 0.05f) {
        LOGR("mouse: camera %08X fov %f (widest %f) from %s", cam, fov, c->widest, where), lines++;
        c->logged = fov;
    }
}

bool ViewZoomed() {
    double now = NowSeconds();
    static bool was = false;
    bool zoomed = false;
    for (auto& e : g_cam_fov)
        if (e.cam && now - e.seen < 0.1 && e.widest >= 1.0f && e.now < e.widest * 0.85f) zoomed = true;
    static int lines = 0;
    if (zoomed != was && lines < 40) LOGR("mouse: zoomed %s", zoomed ? "in" : "out"), lines++;
    was = zoomed;
    return zoomed;
}

void CameraFov(X86Context& ctx, uint8_t*) {
    // the play camera is set: src/common/game_info.cpp switches the controller to its play layout
    game::g_camera_ms = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
    float fov;
    std::memcpy(&fov, HostPtr(ctx.esp), 4);
    if (std::fabs(fov - g_view_fov) > 1.0f) {
        static int lines = 0;
        if (lines < 20) LOGR("mouse: view fov %f", fov), lines++;
    }
    g_view_fov = fov;
    static int logged = 0;
    if (logged < 2) LOGE("camera: fov %f", fov), logged++;  // the game's unit, for a degrees setting later
    float k = std::clamp<uint32_t>(game::g_fov_percent, 50, 200) / 100.0f;
    // Ultrawide: the game culls to its own view, so the camera is widened by the share the window shows more than
    // 16:9; the renderer grows the world back by as much (gpu::g_world_scale), which keeps the game's height
    float uw = gpu::g_ultrawide && gpu::g_squeeze > 0 && gpu::g_squeeze < 1 ? 1 / gpu::g_squeeze : 1.0f;
    gpu::g_world_scale = uw;
    k *= uw;
    if (!(fov > 0)) return;
    // degrees above a half turn in radians
    float half = fov > 3.2f ? fov * 3.14159265f / 360.0f : fov * 0.5f;
    float wide = std::atan(std::tan(half) * k);
    // SSAO's projection (patches/sdk-zzzz-ssao.patch): the view's height as drawn, the world grown back by uw
    gpu::g_view_tan.store(std::tan(wide) / uw, std::memory_order_relaxed);
    if (k == 1) return;
    fov = fov > 3.2f ? wide * 360.0f / 3.14159265f : wide * 2.0f;
    if (fov < 170.0f) std::memcpy(HostPtr(ctx.esp), &fov, 4);
}

// HUD markers over the world (enemy icons, objective distances): the camera's projection of a world point into the
// HUD's 512x448 space, centred (thiscall: camera, world point, out x y z). Ultrawide draws the HUD as 4:3 art and the
// scene as 16:9 (patches/sdk-ultrawide.patch), so a marker's offset from the centre grows by 16:9 over 4:3 to stay on
// what it marks.
X86_EXTERN_FUNC(__imp__sub_00116630);
X86_FUNC(sub_00116630) {
    uint32_t out = R32(ctx.esp + 8);
    __imp__sub_00116630(ctx, base);
    float x, y;
    std::memcpy(&x, HostPtr(out), 4);
    std::memcpy(&y, HostPtr(out + 4), 4);
    // Across: the world is drawn at the window's full width (its squeeze undone by g_world_scale), the HUD as 4:3
    // art in the middle. Down: the camera is widened by g_world_scale both ways and the world grown back by as much.
    if (gpu::g_ultrawide && app::Widescreen() && gpu::g_squeeze > 0) {
        x *= (4.0f / 3.0f) / gpu::g_squeeze;
        y *= gpu::g_world_scale;
        std::memcpy(HostPtr(out), &x, 4);
        std::memcpy(HostPtr(out + 4), &y, 4);
    }
}

// Culling: the camera's frustum planes (0x00118AF0 and 0x00119250, thiscall camera) come from tan(fov / 2), the fov
// in radians at camera+8. Ultrawide shows more at the sides than the game's own view, so for these two the fov is
// widened by the same share: what the frustum keeps then covers the window, the picture is untouched.
namespace {
uint32_t g_cams[16];
int g_cam_count = 0;
struct WiderFrustum {
    uint32_t cam;
    float fov;
    explicit WiderFrustum(uint32_t c) : cam(c) {
        std::memcpy(&fov, HostPtr(cam + 8), 4);
        NoteCamFov(cam, fov, "frustum");
        float k = gpu::g_ultrawide && gpu::g_squeeze < 1 ? 1 / gpu::g_squeeze : 1;
        if (k == 1 || !(fov > 0 && fov < 3.0f)) return;
        float wide = 2 * std::atan(std::tan(fov / 2) * k);
        std::memcpy(HostPtr(cam + 8), &wide, 4);
    }
    ~WiderFrustum() { std::memcpy(HostPtr(cam + 8), &fov, 4); }
};
}  // namespace

X86_EXTERN_FUNC(__imp__sub_00118AF0);
X86_FUNC(sub_00118AF0) {
    WiderFrustum w(ctx.ecx);
    __imp__sub_00118AF0(ctx, base);
}
X86_EXTERN_FUNC(__imp__sub_00119250);
X86_FUNC(sub_00119250) {
    WiderFrustum w(ctx.ecx);
    __imp__sub_00119250(ctx, base);
}

// Finding the gun's camera: every camera that sets its projection (0x001185C0, thiscall camera, fov in degrees,
// ...) is logged once with its fov and caller. The view's comes through 0x00152840 each frame.
X86_EXTERN_FUNC(__imp__sub_001185C0);
X86_FUNC(sub_001185C0) {
    uint32_t* seen = g_cams;
    int& n = g_cam_count;
    uint32_t cam = ctx.ecx, from = R32(ctx.esp);
    bool known = false;
    for (int i = 0; i < n; i++) known |= seen[i] == cam;
    if (!known && n < 16) {
        seen[n++] = cam;
        float fov, a2, a3, a4;
        std::memcpy(&fov, HostPtr(ctx.esp + 4), 4);
        std::memcpy(&a2, HostPtr(ctx.esp + 8), 4);
        std::memcpy(&a3, HostPtr(ctx.esp + 12), 4);
        std::memcpy(&a4, HostPtr(ctx.esp + 16), 4);
        LOGE("camera %d: %08X fov %f args %f %f %f from %08X", n, cam, fov, a2, a3, a4, from);
    }
    {
        float deg;
        std::memcpy(&deg, HostPtr(ctx.esp + 4), 4);
        NoteCamFov(cam, deg * 3.14159265f / 180.0f, "projection");
    }
    {
        // the play camera's near and far planes (its far plane hundreds of units out), for SSAO
        float near_, far_;
        std::memcpy(&near_, HostPtr(ctx.esp + 12), 4);
        std::memcpy(&far_, HostPtr(ctx.esp + 16), 4);
        if (far_ >= 100.0f && near_ > 0.0f && near_ < far_) {
            gpu::g_view_near.store(near_, std::memory_order_relaxed);
            gpu::g_view_far.store(far_, std::memory_order_relaxed);
        }
    }
    __imp__sub_001185C0(ctx, base);
}

// The 2D quad builders (menus, the mission briefing: 0x001FCC90 and four like it) refill one vertex buffer of
// four vertices for every quad, without a lock the library would see. On the console each draw has been read by
// then; here the render thread reads vertex memory later, so every quad of a screen took the last one's texture
// coordinates: the briefing's map, sampled with the text layer's pixel coordinates, came out one flat colour.
// Before the refill, the draws queued so far are read.
void BeforeQuadRefill(X86Context&, uint8_t*) { d3d::FinishQueuedDraws(); }

// Startup intros, off with skip_intro = true (src/common/game_info.cpp). The boot sequence (0x0001BCD0) shows the
// legal screen, a blank screen and the RenderWare screen through 0x00019200 (thiscall, the .sux name or null), each a
// busy wait of 3 seconds, and plays EAbrand.mpc, DICE.mpc and attract.mpc through the movie player.
namespace game {
extern bool g_skip_intro;
}

namespace {

double Seconds() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    return std::chrono::duration<double>(clock::now() - start).count();
}

// an MSVC std::string: its text inline at +4 below 16 characters of capacity (+0x18), else a pointer there
std::string GuestString(uint32_t s) {
    uint32_t cap = R32(s + 0x18), size = R32(s + 0x14);
    uint32_t text = cap >= 16 ? R32(s + 4) : s + 4;
    if (!text || size > 260) return {};
    return std::string(reinterpret_cast<const char*>(HostPtr(text)), size);
}

bool EndsWith(const std::string& s, const char* tail) {
    size_t n = std::strlen(tail);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++)
        if (std::tolower(uint8_t(s[s.size() - n + i])) != std::tolower(uint8_t(tail[i]))) return false;
    return true;
}

}  // namespace

// the boot sequence's screens; nothing else calls it
X86_EXTERN_FUNC(__imp__sub_00019200);
X86_FUNC(sub_00019200) {
    if (game::g_skip_intro) {
        LOGR("intro: screen skipped at %.1fs", Seconds());
        Return(ctx, 0, 1);
        return;
    }
    __imp__sub_00019200(ctx, base);
}

// The movie player (thiscall: std::string name, three flags; returns al, 0 when the movie did not play, which is
// also its own answer when movies are off)
X86_EXTERN_FUNC(__imp__sub_0025E090);
X86_FUNC(sub_0025E090) {
    std::string name = GuestString(R32(ctx.esp + 4));
    static bool attract_seen = false;
    bool intro = EndsWith(name, "EAbrand.mpc") || EndsWith(name, "DICE.mpc") ||
                 (!attract_seen && EndsWith(name, "attract.mpc"));
    if (EndsWith(name, "attract.mpc")) attract_seen = true;
    if (game::g_skip_intro && intro) {
        LOGR("movie: %s skipped at %.1fs", name.c_str(), Seconds());
        Return(ctx, 0, 4);
        return;
    }
    LOGR("movie: %s at %.1fs", name.c_str(), Seconds());
    __imp__sub_0025E090(ctx, base);
    LOGR("movie: %s done at %.1fs", name.c_str(), Seconds());
}

// The front end's state machine (0x0001D600, one state's work a frame, events queued by 0x0001BC90) enters a state
// here (thiscall: the state). Logged with the time, to find where the HQ briefing waits after the level has loaded.
X86_EXTERN_FUNC(__imp__sub_0001BF10);
X86_FUNC(sub_0001BF10) {
    uint32_t from = R32(ctx.ecx + 0x7C), to = R32(ctx.esp + 4);
    if (from != to) LOGR("flow: state %u -> %u at %.1fs", from, to, Seconds());
    __imp__sub_0001BF10(ctx, base);
}

// Mouse aim. The player's input mapping (0x0034FC00) fills the player's controls through 0x0027B4B0 each frame;
// the player's look (0x0002E4D0, thiscall on the player's entity: controls at +0x58, soldier at +0x38) turns the
// soldier by the stick's rates. The soldier's orientation lives in its current state object (soldier +0x188
// indexed by 0x00283F80(soldier +0x174), then +4): yaw +0x24, pitch +0x28 radians, set through 0x00056E10
// (thiscall: yaw, pitch), the game's own setter, which also passes them on. The mouse adds its turn there, before
// the look runs, so the view turns one to one with the mouse: no rate, no dead zone, no curve, no weapon sway from a
// stick, and the stick still works alongside. Right turns the yaw up, down tilts the pitch down.
namespace game {
bool TakeMouse(float& yaw, float& pitch, bool aiming);
bool MouseRight();
}

X86_EXTERN_FUNC(sub_00283F80);
X86_EXTERN_FUNC(sub_00056E10);

namespace {

float F32(uint32_t va) {
    float f;
    std::memcpy(&f, HostPtr(va), 4);
    return f;
}
uint32_t U32(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}

uint32_t g_player_controls = 0;  // the controls the player's mapping fills

// a thiscall into guest code from a hook, the registers kept as they were
uint32_t GuestThiscall(X86Context& ctx, uint8_t* base, void (*fn)(X86Context&, uint8_t*), uint32_t self,
                       std::initializer_list<uint32_t> args) {
    X86Context keep = ctx;
    for (auto p = args.end(); p != args.begin();) {
        ctx.esp -= 4;
        W32(ctx.esp, *--p);
    }
    ctx.esp -= 4;
    W32(ctx.esp, 0xFFFFFFF0u);
    ctx.ecx = self;
    fn(ctx, base);
    uint32_t r = ctx.eax;
    ctx = keep;
    return r;
}

bool Live(uint32_t va) { return va >= 0x10000 && mem::IsCommitted(va) && mem::IsCommitted(va + 0x200); }

}  // namespace

// the player's controls, from the mapping's channel sets
X86_EXTERN_FUNC(__imp__sub_0027B4B0);
X86_FUNC(sub_0027B4B0) {
    uint32_t ret = R32(ctx.esp);
    if (ret == 0x34FCAC || ret == 0x34FCC2) g_player_controls = ctx.ecx;
    __imp__sub_0027B4B0(ctx, base);
}

// Zoomed is the game's own: its view narrowed (ViewZoomed), so toggled aim counts as much as held.
// In the scope the game sways the aim, moving the soldier's pitch (and yaw) a little every frame; against a mouse
// that reads as the view drifting the wrong way. While aiming, firing too (the kick goes with the drift), the
// orientation is held where the mouse left it: each frame starts from the angles written the frame before.
namespace game {
bool MouseLeft();
bool MouseCaptured();
extern bool g_weapon_recoil;
}

X86_EXTERN_FUNC(__imp__sub_0002E4D0);
X86_FUNC(sub_0002E4D0) {
    static uint32_t held_state = 0;
    static float held_yaw = 0, held_pitch = 0;
    uint32_t entity = ctx.ecx;
    if (g_player_controls && Live(entity) && R32(entity + 0x58) == g_player_controls) {
        // the player's soldier is looking: playing, for the HUD layout (patches/sdk-zz-hud-layout.patch)
        gpu::g_hud_play_ms.store(uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count()),
                                 std::memory_order_relaxed);
        float turn = 0, tilt = 0;
        bool aiming = ViewZoomed() || game::MouseRight(), moved = game::TakeMouse(turn, tilt, aiming);
        // held only while the right stick rests (channels 5 and 6), so the stick turns the scope as before
        bool stick = std::fabs(F32(g_player_controls + 4 + 4 * 5)) > 0.01f ||
                     std::fabs(F32(g_player_controls + 4 + 4 * 6)) > 0.01f;
        bool steady = aiming && !stick && game::MouseCaptured();  // firing too: the drift and the kick
        uint32_t soldier = R32(entity + 0x38);
        uint32_t state = 0;
        if ((moved || steady) && Live(soldier)) {
            uint32_t idx = GuestThiscall(ctx, base, sub_00283F80, R32(soldier + 0x174), {});
            uint32_t list = R32(soldier + 0x188);
            uint32_t slot = list ? R32(list + idx * 4) : 0;
            state = slot && Live(slot) ? R32(slot + 4) : 0;
            if (state && !Live(state)) state = 0;
        }
        if (state) {
            constexpr float kPi = 3.14159265f, kLimit = 1.45f;
            bool hold = steady && state == held_state;
            float yaw = hold ? held_yaw : F32(state + 0x24), pitch = hold ? held_pitch : F32(state + 0x28);
            // weapon recoil on: while firing the game's kick up comes through (the gun follows it), its sway and its
            // settling back down do not, so small mouse moves stay true
            if (hold && game::g_weapon_recoil && game::MouseLeft()) pitch += std::max(F32(state + 0x28) - held_pitch, 0.0f);
            yaw = std::remainder(yaw + turn, 2 * kPi);
            pitch = std::clamp(pitch - tilt, -kLimit, kLimit);
            GuestThiscall(ctx, base, sub_00056E10, soldier, {U32(yaw), U32(pitch)});
            held_yaw = F32(state + 0x24), held_pitch = F32(state + 0x28);
        }
        held_state = steady ? state : 0;
    }
    __imp__sub_0002E4D0(ctx, base);
}
