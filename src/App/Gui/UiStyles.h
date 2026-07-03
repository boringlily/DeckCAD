#pragma once
#include "Style.h" // FontId / IconId enums (shared with the Clay path during migration).
#include "Ui.h"
#include <string_view>

// App-side styling layer over src/Ui: text presets mirroring Style.h's TextStyle
// table, sizing shorthands replacing the Clay helper macros in Components.h, and
// the DrawIcon / IconButton composites the views are built from. Header-only;
// every function bottoms out in exported Ui primitives, so it is hot-reload-safe
// to include from App.dll.
namespace UiStyle {

// ── sizing shorthands (Clay Components.h equivalents) ────────────────────────
constexpr Ui::Sizing Expand() { return { Ui::Grow(), Ui::Grow() }; }
constexpr Ui::Sizing ExpandMinMaxWidth(f32 min, f32 max = 0) { return { Ui::Grow(min, max), Ui::Grow() }; }
constexpr Ui::Edges PaddingAll(f32 p) { return { p, p, p, p }; }

// ── text presets (mirror the Style.h TextStyle table) ────────────────────────
// One tiny Label subclass per preset; single shared instances are stateless
// between frames, so stamping them from anywhere is safe.
namespace detail {
    class TitleLabel : public Ui::Label {
        u16 FontId() const override { return static_cast<u16>(::FontId::Semibold); }
        u16 FontSize() const override { return 24; }
        Ui::UiColor TextColor() const override { return Ui::Colors().textBase; }
    };
    class SubtitleLabel : public Ui::Label {
        u16 FontId() const override { return static_cast<u16>(::FontId::Semibold); }
        u16 FontSize() const override { return 16; }
        Ui::UiColor TextColor() const override { return Ui::Colors().textBase; }
    };
    class BodyLabel : public Ui::Label {
        u16 FontId() const override { return static_cast<u16>(::FontId::Regular); }
        u16 FontSize() const override { return 16; }
        Ui::UiColor TextColor() const override { return Ui::Colors().textBase; }
    };
    class CaptionLabel : public Ui::Label {
        u16 FontId() const override { return static_cast<u16>(::FontId::MediumItalic); }
        u16 FontSize() const override { return 16; }
        Ui::UiColor TextColor() const override { return Ui::Colors().textBase; }
    };
    class MutedLabel : public Ui::Label {
        u16 FontId() const override { return static_cast<u16>(::FontId::Regular); }
        u16 FontSize() const override { return 16; }
        Ui::UiColor TextColor() const override { return Ui::Colors().textMuted; }
    };
}

inline void Title(std::string_view s, Ui::UiId id = Ui::kNullId)
{
    static detail::TitleLabel l;
    l.Draw(s, id);
}
inline void Subtitle(std::string_view s, Ui::UiId id = Ui::kNullId)
{
    static detail::SubtitleLabel l;
    l.Draw(s, id);
}
inline void Body(std::string_view s, Ui::UiId id = Ui::kNullId)
{
    static detail::BodyLabel l;
    l.Draw(s, id);
}
inline void Caption(std::string_view s, Ui::UiId id = Ui::kNullId)
{
    static detail::CaptionLabel l;
    l.Draw(s, id);
}
inline void Muted(std::string_view s, Ui::UiId id = Ui::kNullId)
{
    static detail::MutedLabel l;
    l.Draw(s, id);
}

// ── icons ─────────────────────────────────────────────────────────────────────
// Fixed-size icon with an explicit per-call tint (replaces Graphics' DrawIcon).
// Decorative: never hit-testable, so it can sit inside buttons freely.
inline void DrawIcon(IconId icon, Ui::UiColor tint, f32 size = 24)
{
    Ui::LayoutConfig c {};
    c.sizing = { Ui::Fixed(size), Ui::Fixed(size) };
    c.hitTestable = false;
    u32 n = Ui::OpenElement(c, Ui::kNullId);
    Ui::ConfigureIcon(n, static_cast<s32>(icon), tint);
    Ui::CloseElement();
}

// ── icon + label button (the app's dominant widget) ──────────────────────────
struct IconButtonStyle {
    bool active { false }; // force the highlighted background (current layer/tab/tool).
    f32 cornerRadius { 8 };
    f32 gap { 8 };
    Ui::Edges border {}; // optional border widths (e.g. active tool outline).
    // Base/hover backgrounds; alpha==0 means "use the theme default" (bgDark/bgLight).
    Ui::UiColor bg { 0, 0, 0, 0 };
    Ui::UiColor bgHover { 0, 0, 0, 0 };
};

// Icon + left-aligned label; returns true on the frame it is clicked. The id must
// be content-stable (NameId) so hover/click state survives hot-reloads.
inline bool IconButton(IconId icon, std::string_view label, Ui::UiId id, const IconButtonStyle& style = {})
{
    const Ui::ColorScheme& colors = Ui::Colors();
    const Ui::UiColor base = style.bg.a ? style.bg : colors.bgDark;
    const Ui::UiColor hover = style.bgHover.a ? style.bgHover : colors.bgLight;

    Ui::LayoutConfig c {};
    c.sizing = { Ui::Fit(), Ui::Fit() };
    c.padding = PaddingAll(4);
    c.gap = style.gap;
    c.justify = Ui::Justify::Start;
    c.align = Ui::AlignCross::Center;
    c.background = (style.active || Ui::IsHovered(id)) ? hover : base;
    c.cornerRadius = style.cornerRadius;
    c.border = style.border;
    c.borderColor = colors.borderBase;
    Ui::OpenElement(c, id);

    DrawIcon(icon, colors.textBase);

    if (!label.empty()) {
        // Label child is decorative; the button itself captures the click.
        Ui::LayoutConfig textCfg {};
        textCfg.hitTestable = false;
        u32 n = Ui::OpenElement(textCfg, Ui::HashChild(id, 1));
        Ui::ConfigureText(n, label.data(), static_cast<u32>(label.size()),
            static_cast<u16>(::FontId::Regular), 16, colors.textBase);
        Ui::CloseElement();
    }

    Ui::CloseElement();
    return Ui::IsClicked(id);
}

} // namespace UiStyle
