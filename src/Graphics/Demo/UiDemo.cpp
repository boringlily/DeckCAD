// Standalone showcase for the Ui framework: borders, a checkbox, a text field,
// a floating dropdown, a floating modal dialog, and icons - all drawn by the
// Ui framework in src/Graphics (no Clay). Run with --shot to capture
// verification screenshots.

#include "Ui.h"
#include "raylib.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

using namespace Ui;

// Directory for --shot screenshots, injected by CMake to a path under the build
// tree. Fallback to the working directory if built without the define.
#ifndef UIDEMO_SHOT_DIR
#define UIDEMO_SHOT_DIR "."
#endif

// Write the current frame to UIDEMO_SHOT_DIR/<name>, creating the directory if
// needed. Bypasses raylib's TakeScreenshot, which writes relative to the process
// working directory (so shots would otherwise land wherever the exe was launched).
static void SaveShot(const char* name)
{
    std::error_code ec;
    std::filesystem::create_directories(UIDEMO_SHOT_DIR, ec);
    std::string path = std::string(UIDEMO_SHOT_DIR) + "/" + name;
    Image img = LoadImageFromScreen();
    ExportImage(img, path.c_str());
    UnloadImage(img);
}

// ----- App-side icon ids (order matches the textures loaded below) -----------
enum DemoIcon : s32 { Icon_Home = 0,
    Icon_Settings,
    Icon_Plus,
    Icon_Check,
    Icon_Count };

struct DemoState {
    bool wireframe { true };
    bool snap { false };
    char name[64] { "Sketch_01" };
    u32 nameLen { 9 };
    char notes[256] { "Sketch on the XY plane.\nFully constrain before extruding the profile." };
    u32 notesLen { 0 };
    char eqOk[96] { "width * cos(t) + 3.5" };
    u32 eqOkLen { 0 };
    char eqErr[96] { "height + @ * 2" };
    u32 eqErrLen { 0 };
    bool dropdownOpen { false };
    bool modalOpen { false };
    int selectedTool { 0 };
    int selectedLayer { 2 };
};

static const char* kTools[] = { "Line", "Arc", "Circle", "Rectangle" };
static const char* kLayers[] = { "Origin", "Base Sketch", "Extrude 1", "Fillet 1",
    "Chamfer 1", "Hole Pattern", "Shell", "Draft", "Mirror 1", "Sketch 2",
    "Extrude 2", "Rib", "Thread", "Construction" };
constexpr int kLayerCount = sizeof(kLayers) / sizeof(kLayers[0]);

// Example user parser: a tiny tokenizer for parametric-equation-style input. It
// colorizes numbers / identifiers / operators / parens, underlines invalid
// characters, and reports unbalanced parentheses - exactly the hook a real
// equation editor would plug a proper parser into.
static void ExpressionParser(void* /*user*/, const char* text, u32 len, Ui::TextParseResult* out)
{
    using namespace Ui;
    auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    auto isAlpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    auto isAlnum = [&](char c) { return isAlpha(c) || isDigit(c); };

    const UiColor kNum { 60, 130, 200, 255 };
    const UiColor kIdent { 150, 80, 200, 255 };
    const UiColor kOp { 190, 110, 40, 255 };
    const UiColor kParen { 110, 110, 110, 255 };
    const UiColor kErr { 220, 64, 64, 255 };

    u32 n = 0;
    int depth = 0;
    bool bad = false;
    const char* msg = nullptr;
    for (u32 i = 0; i < len && n < out->runCap;) {
        char c = text[i];
        if (c == ' ' || c == '\t') {
            ++i;
            continue;
        }
        if (isDigit(c) || c == '.') {
            u32 start = i;
            while (i < len && (isDigit(text[i]) || text[i] == '.')) {
                ++i;
            }
            out->runs[n++] = { start, i - start, kNum, TextDecoration::None };
        } else if (isAlpha(c)) {
            u32 start = i;
            while (i < len && isAlnum(text[i])) {
                ++i;
            }
            out->runs[n++] = { start, i - start, kIdent, TextDecoration::None };
        } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '=' || c == ',') {
            out->runs[n++] = { i, 1, kOp, TextDecoration::None };
            ++i;
        } else if (c == '(') {
            ++depth;
            out->runs[n++] = { i, 1, kParen, TextDecoration::None };
            ++i;
        } else if (c == ')') {
            --depth;
            if (depth < 0 && !bad) {
                bad = true;
                msg = "Unexpected ')'";
            }
            out->runs[n++] = { i, 1, kParen, TextDecoration::None };
            ++i;
        } else {
            if (!bad) {
                bad = true;
                msg = "Invalid character in expression";
            }
            out->runs[n++] = { i, 1, kErr, TextDecoration::Error };
            ++i;
        }
    }
    if (!bad && depth > 0) {
        bad = true;
        msg = "Unbalanced parentheses: missing ')'";
    }
    out->runCount = n;
    out->hasError = bad;
    out->message = bad ? msg : nullptr;
}

// A 3D viewport: subclass Canvas3D and draw a raylib scene. The framework renders
// this into a RenderTexture sized to the element's rect and composites it.
class Viewport : public Ui::Raylib::Canvas3D {
public:
    void Draw3D(Ui::Rect) override
    {
        DrawGrid(20, 1.0f);
        DrawCube(Vector3 { 0, 1, 0 }, 2, 2, 2, Color { 172, 153, 255, 255 });
        DrawCubeWires(Vector3 { 0, 1, 0 }, 2, 2, 2, Color { 80, 60, 140, 255 });
        DrawLine3D(Vector3 { 0, 0, 0 }, Vector3 { 4, 0, 0 }, Color { 220, 64, 64, 255 }); // X
        DrawLine3D(Vector3 { 0, 0, 0 }, Vector3 { 0, 4, 0 }, Color { 90, 190, 110, 255 }); // Y
        DrawLine3D(Vector3 { 0, 0, 0 }, Vector3 { 0, 0, 4 }, Color { 90, 160, 230, 255 }); // Z
    }
};
static Viewport gViewport;

static void BuildShowcase(DemoState& s)
{
    const ColorScheme& col = Colors();

    // Root: fills the window, padded, vertical.
    LayoutConfig root {};
    root.sizing = { Grow(), Grow() };
    root.direction = Direction::TopToBottom;
    root.align = AlignCross::Stretch; // body row fills the window width.
    root.padding = { 24, 24, 24, 24 };
    root.gap = 16;
    root.background = col.bgDark;
    OpenElement(root, HashId("root", 0));

    Text("Ui Framework - Widgets", 24);

    // Body row: the widgets card (left) next to a 3D viewport (right). Fit height
    // so the row is as tall as the card; Stretch so the viewport matches it.
    LayoutConfig body {};
    body.direction = Direction::LeftToRight;
    body.sizing = { Grow(), Fit() };
    body.align = AlignCross::Stretch;
    body.gap = 16;
    OpenElement(body, HashId("body", 0));

    // Card with a border holding the widgets.
    LayoutConfig card {};
    card.sizing = { Fixed(440), Fit() };
    card.direction = Direction::TopToBottom;
    card.padding = { 20, 20, 20, 20 };
    card.gap = 14;
    card.align = AlignCross::Stretch; // inputs/paragraphs fill the card width.
    card.cornerRadius = 8;
    card.background = col.bgBase;
    card.border = { 1, 1, 1, 1 };
    card.borderColor = col.borderBase;
    OpenElement(card, HashId("card", 0));

    // Icon row.
    BeginRow(HashId("iconrow", 0), 12);
    {
        Icon icon;
        icon.Draw(Icon_Home, HashId("ic", 0));
        icon.Draw(Icon_Settings, HashId("ic", 1));
        icon.Draw(Icon_Plus, HashId("ic", 2));
        icon.Draw(Icon_Check, HashId("ic", 3));
    }
    EndRow();

    // Checkboxes.
    Checkbox(s.wireframe, "Wireframe", HashId("cb", 0));
    Checkbox(s.snap, "Snap to grid", HashId("cb", 1));

    // Wrapping paragraph (reflows to the card width).
    Paragraph("This panel is laid out and drawn entirely by the Ui framework - a flexbox "
              "immediate-mode GUI with floating panels, scrolling, and word-wrapped text. No Clay.",
        14, col.textMuted, HashId("para", 0));

    // Single-line text field.
    Text("Sketch name", 14, col.textMuted);
    InputLabel(s.name, s.nameLen, sizeof(s.name), "Enter a name...", HashId("tf", 0));

    // Multi-line input box (wraps, multi-line caret editing, vertical scroll).
    Text("Notes", 14, col.textMuted);
    InputBox(s.notes, s.notesLen, sizeof(s.notes), "Notes...", HashId("ta", 0), 76);

    // Parser-driven text boxes: live syntax highlighting + error display.
    Text("Parametric equation", 14, col.textMuted);
    TextParser parser { nullptr, ExpressionParser };
    InputLabel(s.eqOk, s.eqOkLen, sizeof(s.eqOk), "expression", HashId("eqok", 0), parser);
    InputLabel(s.eqErr, s.eqErrLen, sizeof(s.eqErr), "expression", HashId("eqerr", 0), parser);

    // Dropdown + a button that opens the modal, on one row.
    BeginRow(HashId("ctlrow", 0), 12);
    {
        if (BeginDropdown(s.dropdownOpen, kTools[s.selectedTool], HashId("dd", 0))) {
            for (int i = 0; i < 4; ++i) {
                if (DropdownItem(kTools[i], HashId("ddi", static_cast<u32>(i)))) {
                    s.selectedTool = i;
                    s.dropdownOpen = false;
                }
            }
            EndDropdown();
        }

        static Button openBtn;
        openBtn.Draw(
            "Open Dialog", [&]() { s.modalOpen = true; }, HashId("openbtn", 0));
    }
    EndRow();

    // Scrollable list (content taller than the panel -> wheel-scrolls + clips).
    Text("Feature tree", 14, col.textMuted);
    BeginScrollPanel(HashId("scroll", 0), 150, 400);
    for (int i = 0; i < kLayerCount; ++i) {
        if (ListItem(kLayers[i], i == s.selectedLayer, HashId("li", static_cast<u32>(i)))) {
            s.selectedLayer = i;
        }
    }
    EndScrollPanel();

    CloseElement(); // card

    // 3D viewport fills the rest of the body row.
    gViewport.Render(HashId("viewport", 0));

    CloseElement(); // body
    CloseElement(); // root

    // Floating modal (drawn above everything via its own layer).
    MessageBox("Delete sketch?",
        "This permanently removes the sketch and every feature that depends on it. "
        "This action cannot be undone.",
        s.modalOpen, HashId("modal", 0));
}

static KeyboardState ReadKeyboard()
{
    KeyboardState kb {};
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    kb.shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (!ctrl) { // Ctrl combos are actions, not typed text.
        int cp;
        while ((cp = GetCharPressed()) != 0 && kb.typedCount < 16) {
            kb.typed[kb.typedCount++] = static_cast<u32>(cp);
        }
    }
    kb.copy = ctrl && IsKeyPressed(KEY_C);
    kb.cut = ctrl && IsKeyPressed(KEY_X);
    kb.paste = ctrl && IsKeyPressed(KEY_V);
    kb.selectAll = ctrl && IsKeyPressed(KEY_A);
    kb.backspace = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE);
    kb.del = IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE);
    kb.enter = IsKeyPressed(KEY_ENTER);
    kb.escape = IsKeyPressed(KEY_ESCAPE);
    kb.left = IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT);
    kb.right = IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT);
    kb.up = IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP);
    kb.down = IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN);
    kb.home = IsKeyPressed(KEY_HOME);
    kb.end = IsKeyPressed(KEY_END);
    return kb;
}

static PointerState ReadPointer()
{
    Vector2 m = GetMousePosition();
    Vector2 w = GetMouseWheelMoveV();
    PointerState p {};
    p.pos = { m.x, m.y };
    p.wheel = { w.x, w.y };
    p.down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    p.pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    p.released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    return p;
}

int main(int argc, char** argv)
{
    bool shotMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--shot") {
            shotMode = true;
        }
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(980, 1000, "Ui Framework Demo");
    SetWindowMinSize(820, 680);

    static std::array<Font, 2> fonts;
    fonts[0] = LoadFontEx("../assets/fonts/Nunito/static/Nunito-Regular.ttf", 32, nullptr, 127);
    fonts[1] = LoadFontEx("../assets/fonts/Nunito/static/Nunito-SemiBold.ttf", 32, nullptr, 127);
    if (fonts[0].glyphs) {
        SetTextureFilter(fonts[0].texture, TEXTURE_FILTER_BILINEAR);
    }

    // LoadIcon prefers <base>.svg (rasterized via nanosvg) over <base>.png.
    static std::array<Texture2D, Icon_Count> icons;
    const char* iconBases[Icon_Count] = { "../assets/Icon/Home", "../assets/Icon/Settings",
        "../assets/Icon/Plus", "../assets/Icon/Check" };
    for (int i = 0; i < Icon_Count; ++i) {
        icons[i] = Ui::Raylib::LoadIcon(iconBases[i], 48);
    }
    // 48x48 means the SVG was rasterized; a PNG would keep its native size.
    std::printf("[icons] Home=%dx%d Settings=%dx%d (48x48 => loaded from SVG)\n",
        icons[Icon_Home].width, icons[Icon_Home].height,
        icons[Icon_Settings].width, icons[Icon_Settings].height);

    Ui::Raylib::State backendState { fonts.data(), static_cast<u32>(fonts.size()),
        icons.data(), static_cast<u32>(icons.size()) };
    UiBackend backend = Ui::Raylib::MakeBackend(&backendState);

    static std::array<u8, 1u << 22> uiBuffer; // 4 MB, user-owned.
    Context ctx;
    UiInitDesc desc {};
    desc.buffer = uiBuffer.data();
    desc.bufferBytes = uiBuffer.size();
    desc.maxNodes = 4096;
    desc.maxCommands = 8192;
    desc.backend = backend;
    if (!ctx.Init(desc)) {
        std::printf("Ui Context init failed (buffer too small).\n");
        return 1;
    }
    Ui::SetCurrent(&ctx);

    DemoState state;
    state.notesLen = static_cast<u32>(std::strlen(state.notes));
    state.eqOkLen = static_cast<u32>(std::strlen(state.eqOk));
    state.eqErrLen = static_cast<u32>(std::strlen(state.eqErr));
    int frame = 0;
    while (!WindowShouldClose()) {
        Vec2 rootSize { static_cast<f32>(GetScreenWidth()), static_cast<f32>(GetScreenHeight()) };

        if (shotMode) {
            // Scripted verification of widgets, modal, and the text-input editing:
            // click-to-position caret, multi-line edit, shift-select + highlight,
            // and clipboard copy/paste.
            PointerState p {};
            KeyboardState kb {};
            if (frame == 5) {
                state.modalOpen = true;
            }
            if (frame == 8) {
                state.modalOpen = false;
            }
            // Checkbox composite hit-test.
            if (frame == 9) {
                p.pos = { 95, 179 };
            }
            if (frame == 10) {
                p.pos = { 95, 179 };
                p.pressed = true;
                p.down = true;
            }
            // Click-to-position: hover the name field, click near its LEFT edge
            // (caret -> 0), then type. Tests that the click sets the caret, not the end.
            if (frame == 13) {
                p.pos = { 200, 288 };
            }
            if (frame == 14) {
                p.pos = { 55, 288 };
                p.pressed = true;
                p.down = true;
            }
            if (frame == 15) {
                kb.typed[0] = 'Q';
                kb.typedCount = 1;
            }
            // InputBox multi-line: focus Notes, caret Up, Home, insert 'Z' at line 0.
            if (frame == 17) {
                p.pos = { 240, 383 };
            }
            if (frame == 18) {
                p.pos = { 240, 383 };
                p.pressed = true;
                p.down = true;
            }
            if (frame == 19) {
                kb.up = true;
            }
            if (frame == 20) {
                kb.home = true;
            }
            if (frame == 21) {
                kb.typed[0] = 'Z';
                kb.typedCount = 1;
            }
            // Selection + clipboard on the name field: focus, Home, Shift+Right x6 to
            // select the first 6 chars, copy, jump End, paste at the end.
            if (frame == 25) {
                p.pos = { 200, 288 };
            }
            if (frame == 26) {
                p.pos = { 200, 288 };
                p.pressed = true;
                p.down = true;
            }
            if (frame == 27) {
                kb.home = true;
            }
            if (frame >= 28 && frame <= 33) {
                kb.shift = true;
                kb.right = true;
            }
            if (frame == 35) {
                kb.copy = true;
            }
            if (frame == 36) {
                kb.end = true;
            }
            if (frame == 37) {
                kb.paste = true;
            }
            Ui::BeginFrame(rootSize, p, kb);
        } else {
            Ui::BeginFrame(rootSize, ReadPointer(), ReadKeyboard());
        }

        BeginDrawing();
        ClearBackground(Color { 30, 30, 30, 255 });
        BuildShowcase(state);
        Ui::EndFrame();
        EndDrawing();

        if (frame == 2) {
            std::printf("[ui] nodes=%u commands=%u floats=%u persistent=%llu transient_high=%llu\n",
                ctx.nodeCount, ctx.commands.count, ctx.floatCount,
                static_cast<unsigned long long>(ctx.arena.offset),
                static_cast<unsigned long long>(ctx.transient.highWater));
        }
        if (shotMode && frame == 3) {
            SaveShot("ui_demo_widgets.png");
        }
        if (shotMode && frame == 7) {
            SaveShot("ui_demo_modal.png");
        }
        if (shotMode && frame == 11) {
            std::printf("[checkbox] snap = %s (expected ON)\n", state.snap ? "ON" : "OFF");
        }
        if (shotMode && frame == 16) {
            std::printf("[click-to-pos] name = \"%s\" (expected to start with 'Q')\n", state.name);
        }
        if (shotMode && frame == 23) {
            std::printf("[box-multiline] notes start = '%c' (expected 'Z')\n", state.notes[0]);
        }
        if (shotMode && frame == 34) {
            std::printf("[selection] name = \"%s\", first 6 chars highlighted\n", state.name);
            SaveShot("ui_demo_selection.png");
        }
        if (shotMode && frame == 38) {
            std::printf("[clipboard] name = \"%s\" (expected to end with \"%.6s\")\n", state.name, state.name);
            SaveShot("ui_demo_clipboard.png");
            break;
        }
        frame++;
    }

    gViewport.Unload(); // free the render texture while the GL context is live.
    CloseWindow();
    return 0;
}
