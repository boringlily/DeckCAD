#include "Widgets.h"

namespace Ui
{

void IconImage(const IconSet& icons_ref, IconId icon, f32 size, const Color& tint_ref)
{
    if(!icons_ref.isValid())
    {
        ImGui::Dummy(ImVec2(size, size));
        return;
    }

    f32 uv0[2] {};
    f32 uv1[2] {};
    icons_ref.getUvRange(icon, uv0, uv1);

    // Image() lost its tint parameter in 1.91.9; ImageWithBg() carries it now
    ImGui::ImageWithBg(Ui::ToImTextureID(icons_ref.getTextureHandle()),
        ImVec2(size, size),
        ImVec2(uv0[0], uv0[1]),
        ImVec2(uv1[0], uv1[1]),
        ImVec4(0, 0, 0, 0),
        ToImVec4(tint_ref));
}

bool IconButton(const IconSet& icons_ref, IconId icon, const char* id_ptr, f32 size, const Color& tint_ref)
{
    if(!icons_ref.isValid())
    {
        return ImGui::Button(id_ptr, ImVec2(size, size));
    }

    f32 uv0[2] {};
    f32 uv1[2] {};
    icons_ref.getUvRange(icon, uv0, uv1);

    ImGui::PushID(id_ptr);
    const bool clicked = ImGui::ImageButton(id_ptr,
        Ui::ToImTextureID(icons_ref.getTextureHandle()),
        ImVec2(size, size),
        ImVec2(uv0[0], uv0[1]),
        ImVec2(uv1[0], uv1[1]),
        ImVec4(0, 0, 0, 0), // transparent background; the style supplies hover/active
        ToImVec4(tint_ref));
    ImGui::PopID();
    return clicked;
}

bool TabButton(const char* label_ptr, bool selected)
{
    // forces the "active" colour while selected, instead of a separate pressed-tab widget
    if(selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    const bool clicked = ImGui::Button(label_ptr);
    if(selected)
    {
        ImGui::PopStyleColor();
    }
    return clicked;
}

void CenterNextItem(f32 item_width)
{
    const f32 available = ImGui::GetContentRegionAvail().x;
    if(available > item_width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available - item_width) * 0.5f);
    }
}

} // namespace Ui
