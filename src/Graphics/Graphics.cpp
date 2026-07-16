#define RAYLIB_IMPLEMENTATION
#include "raylib.h"
#include "raymath.h"

#include "stdint.h"
#include "cstring"
#include "stdio.h"
#include "stdlib.h"

#include <string>
#include <array>
#include <vector>
#include <print>
#include <format>

#include "Graphics.h"

#include <tracy/Tracy.hpp>

#include "Ui.h"

#define ASSETS_PATH "../assets/"
#define ICONS_PATH ASSETS_PATH "Icon/"
#define FONTS_PATH ASSETS_PATH "fonts/Nunito/static/"

static std::array<Font, 4> loadedFonts;

static inline void LoadAppFonts()
{
    struct FontLoadConfig {
        FontId id;
        u32 loadSizePixels;
        u8 filename[26];
    };
    static constexpr std::array<FontLoadConfig, 4u> fontLoadList {
        FontLoadConfig { .id = FontId::Regular, .loadSizePixels = 16, .filename = "Nunito-Regular.ttf" },
        FontLoadConfig { .id = FontId::Medium, .loadSizePixels = 16, .filename = "Nunito-Medium.ttf" },
        FontLoadConfig { .id = FontId::MediumItalic, .loadSizePixels = 16, .filename = "Nunito-MediumItalic.ttf" },
        FontLoadConfig { .id = FontId::Semibold, .loadSizePixels = 24, .filename = "Nunito-SemiBold.ttf" },
    };

    // Load at higher scale if display is a retina.
    f64 render_scale = static_cast<f64>(GetRenderWidth()) / GetScreenWidth();
    if (render_scale < 1) {
        render_scale = 1;
    }
    for (auto& font : fontLoadList) {
        static std::array<char, 60> fullpath;
        snprintf(const_cast<char*>(fullpath.data()), 60, FONTS_PATH "%s", font.filename);
        u8 index = static_cast<u8>(font.id);
        loadedFonts[index] = LoadFontEx(fullpath.data(), font.loadSizePixels * render_scale, nullptr, 127);
        SetTextureFilter(loadedFonts[index].texture, TEXTURE_FILTER_BILINEAR);
    }
}

struct Icon {
    std::array<u8, 15> name;
    Texture2D texture {};
};

#define MAKE_STRINGS(VAR) #VAR,
const std::array<std::array<u8, 20>, 9> IconNames = {
    Make_Icons(MAKE_STRINGS)
};

// ─── Ui runtime ────────────────────────────────────────────────────────────────
// The Ui context, its backing buffer, the backend state (fonts/icons), and the
// path flag are owned by the Graphics module so they survive App.dll hot-reloads
// (same reason Clay's arena lives here). Fonts are shared with the Clay path;
// icons load separately because the Ui backend prefers the SVG assets.
static std::array<u8, 1u << 22> uiMemory; // 4 MB caller-owned arena buffer.
static Ui::Context uiContext;
static Ui::Raylib::State uiBackendState;
static std::array<Texture2D, IconNames.size()> uiIcons;
static bool uiReady { false };

static void LoadUiIcons()
{
    u8 index { 0 };
    for (auto& name : IconNames) {
        static std::array<char, 60> iconPath;
        // Base path without extension: LoadIcon tries <base>.svg then <base>.png.
        snprintf(iconPath.data(), iconPath.size(), ICONS_PATH "%s", name.data());
        uiIcons[index] = Ui::Raylib::LoadIcon(iconPath.data(), 48);
        index++;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

GRAPHICS_API
void Graphics::Initialize()
{
    constexpr int WINDOW_MIN_WIDTH = 1440;
    constexpr int WINDOW_MIN_HEIGHT = 744;
    std::string WINDOW_TITLE = "DeckCAD";
    SetConfigFlags(
        FLAG_WINDOW_RESIZABLE
        | FLAG_WINDOW_HIGHDPI
        | FLAG_WINDOW_HIGHDPI
        | FLAG_MSAA_4X_HINT
        | FLAG_VSYNC_HINT);

    InitWindow(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT, const_cast<char*>(WINDOW_TITLE.c_str()));
    SetWindowMinSize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);

    SetExitKey(KEY_NULL); // Disable exit key

    LoadAppFonts();

    // Ui runtime — the sole UI layer. Reuses the fonts loaded above.
    LoadUiIcons();
    uiBackendState = Ui::Raylib::State {
        loadedFonts.data(), static_cast<u32>(loadedFonts.size()),
        uiIcons.data(), static_cast<u32>(uiIcons.size())
    };
    Ui::UiInitDesc uiDesc {};
    uiDesc.buffer = uiMemory.data();
    uiDesc.bufferBytes = uiMemory.size();
    uiDesc.maxNodes = 4096;
    uiDesc.maxCommands = 8192;
    uiDesc.maxScrollStates = 256;
    // No explicit ColorScheme: MakeBackend's default is Ui::ColorScheme{}, the
    // app's single theme (Ui/Backend/IBackend.h) — every component reads the same
    // instance via Ui::Colors(), so there is nothing to keep in sync here anymore.
    uiDesc.backend = Ui::Raylib::MakeBackend(&uiBackendState);
    uiReady = uiContext.Init(uiDesc);
    if (uiReady) {
        Ui::SetCurrent(&uiContext);
    } else {
        printf("FATAL: Ui context init failed (buffer too small?) — no UI will render.\n");
    }
}

GRAPHICS_API
void Graphics::Deactivate()
{
    CloseWindow();
}

GRAPHICS_API
void Graphics::BeginFrame()
{
    ZoneScoped;

    // Keyboard is sampled here, not just in the demo: without it KeyboardState stays
    // default-constructed and every editable field (Ui::InputLabel / InputBox) renders
    // and takes focus but can never receive a character.
    Ui::PointerState pointer = Ui::Raylib::ReadPointer();
    Ui::KeyboardState keyboard = Ui::Raylib::ReadKeyboard();
    Ui::BeginFrame({ static_cast<f32>(GetScreenWidth()), static_cast<f32>(GetScreenHeight()) }, pointer, keyboard);
}

GRAPHICS_API
void Graphics::EndFrame()
{
    ZoneScoped;

    // Ui::EndFrame dispatches draw calls immediately, so it must run inside the
    // raylib frame. FrameMark marks one rendered frame for Tracy's frame-time graph.
    BeginDrawing();
    ClearBackground(BLACK);
    Ui::EndFrame(); // solve -> resolve input -> emit -> dispatch.
    EndDrawing();

    FrameMark;
}

GRAPHICS_API
void Graphics::OnAppReloaded()
{
    // Ids can churn across a reload until every element uses content-stable
    // NameIds; drop stale per-id scroll offsets so the table can't silently fill.
    if (uiReady) {
        Ui::ClearScrollStates(uiContext);
    }
}

#ifdef __cplusplus
}
#endif

/// TODO: Move this into the canvas logic space, I think it belongs there.

// Get a ray trace from the screen position (i.e mouse) within a specific section of the screen
Ray GetScreenToWorldPointWithZDistance(Vector2 position, Camera3D camera, int screenWidth, int screenHeight, float zDistance)
{
    Ray ray = { 0 };

    // Calculate normalized device coordinates
    // NOTE: y value is negative
    float x = (2.0f * position.x) / (float)screenWidth - 1.0f;
    float y = 1.0f - (2.0f * position.y) / (float)screenHeight;
    float z = 1.0f;

    // Store values in a vector
    Vector3 deviceCoords = { x, y, z };

    // Calculate view matrix from camera look at
    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE) {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy * DEG2RAD, ((double)screenWidth / (double)screenHeight), 0.01f, zDistance);
    } else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        double aspect = (double)screenWidth / (double)screenHeight;
        double top = camera.fovy / 2.0;
        double right = top * aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    }

    // Unproject far/near points
    Vector3 nearPoint = Vector3Unproject((Vector3) { deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
    Vector3 farPoint = Vector3Unproject((Vector3) { deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);

    // Calculate normalized direction vector
    Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

    ray.position = farPoint;

    // Apply calculated vectors to ray
    ray.direction = direction;

    return ray;
}