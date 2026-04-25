#pragma once
#include "Icons.h"
#include "Theme.h"
#include "Types.h"
#include <cstdint>
#include <imgui.h>

namespace Ui {

inline ImVec4 ToImVec4(const Color& color_ref)
{
    DeckMath::Vector4 v = color_ref.toVector4();
    return ImVec4 { v.x, v.y, v.z, v.w };
}

inline ImU32 ToImU32(const Color& color_ref)
{
    return IM_COL32(color_ref.r, color_ref.g, color_ref.b, color_ref.a);
}

/// ImTextureID is a 64-bit integer, so a texture view pointer has to go through
/// an integer cast rather than reinterpret_cast.
inline ImTextureID ToImTextureID(void* handle_ptr)
{
    return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(handle_ptr));
}

/// Draws an icon from the atlas at @p size logical points, tinted.
void IconImage(const IconSet& icons_ref, IconId icon, f32 size, const Color& tint_ref);

/// Icon-only button. Returns true on click.
bool IconButton(const IconSet& icons_ref, IconId icon, const char* id_ptr, f32 size, const Color& tint_ref);

/// A tab-style button that renders "pressed" while @p selected is true.
bool TabButton(const char* label_ptr, bool selected);

/// Centres the next item horizontally within the available content region.
void CenterNextItem(f32 item_width);

} // namespace Ui
