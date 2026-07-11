#pragma once
#include "Style.h" // FontId / IconId asset-id enums Graphics loads at boot.
#include "Ui.h"
#include <string_view>

// The app's style toolbox: everything App.cpp reaches for to draw themed UI.
// Composes Graphics' asset ids (FontId/IconId, Style.h) and Ui's single palette
// (Ui::ColorScheme, read live via Ui::Colors()) into text presets, sizing
// shorthands, and the Button/icon primitives the views are built from. This is
// the one place to add more of either — a new text preset, a new ButtonStyle
// variant, a new named ColorScheme for a future theme. Header-only; every
// function bottoms out in exported Ui primitives, so it is hot-reload-safe to
// include from App.dll.
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

// Non-hit-testable text leaf (decorative): lets a parent button/tab capture the
// click instead of the text stealing it. `font` is the app-side FontId enum.
inline void DecorText(std::string_view s, ::FontId font, u16 size, Ui::UiColor color, Ui::UiId id = Ui::kNullId)
{
    Ui::LayoutConfig c {};
    c.hitTestable = false;
    u32 n = Ui::OpenElement(c, id);
    Ui::ConfigureText(n, s.data(), static_cast<u32>(s.size()), static_cast<u16>(font), size, color);
    Ui::CloseElement();
}

// ── button (the app's dominant widget) ───────────────────────────────────────
// One flexible primitive covering the header/tab/tool buttons: optional leading
// icon + optional label, per-state background, optional border, returns clicked.
// Colors with alpha==0 fall back to a theme default. The id MUST be content-stable
// (NameId / HashId) so hover/click state survives hot-reloads.
struct ButtonStyle {
    int icon { -1 }; // an IconId to draw before the label, or -1 for none.
    Ui::Sizing sizing { Ui::Fit(), Ui::Fit() };
    Ui::Justify justify { Ui::Justify::Start }; // Start = left-align icon+label.
    Ui::Edges padding { 4, 4, 4, 4 };
    f32 gap { 8 };
    f32 cornerRadius { 8 };
    Ui::Edges border {};
    Ui::UiColor borderColor { 0, 0, 0, 0 }; // a==0 -> colors.borderBase
    bool active { false }; // force the highlighted (hover) background.
    Ui::UiColor bg { 0, 0, 0, 0 }; // resting bg;  a==0 -> colors.bgBase
    Ui::UiColor bgHover { 0, 0, 0, 0 }; // hover/active bg; a==0 -> colors.bgLight
    Ui::UiColor labelColor { 0, 0, 0, 0 }; // a==0 -> colors.textBase
};

inline bool Button(std::string_view label, Ui::UiId id, const ButtonStyle& s = {})
{
    const Ui::ColorScheme& colors = Ui::Colors();
    Ui::LayoutConfig c {};
    c.sizing = s.sizing;
    c.padding = s.padding;
    c.gap = s.gap;
    c.justify = s.justify;
    c.align = Ui::AlignCross::Center;
    c.cornerRadius = s.cornerRadius;
    c.border = s.border;
    c.borderColor = s.borderColor.a ? s.borderColor : colors.borderBase;
    c.background = (s.active || Ui::IsHovered(id))
        ? (s.bgHover.a ? s.bgHover : colors.bgLight)
        : (s.bg.a ? s.bg : colors.bgBase);
    Ui::OpenElement(c, id);
    if (s.icon >= 0) {
        DrawIcon(static_cast<IconId>(s.icon), colors.textBase);
    }
    if (!label.empty()) {
        // Decorative children — the button element itself captures the click.
        DecorText(label, ::FontId::Regular, 16, s.labelColor.a ? s.labelColor : colors.textBase, Ui::HashChild(id, 1));
    }
    Ui::CloseElement();
    return Ui::IsClicked(id);
}

} // namespace UiStyle
