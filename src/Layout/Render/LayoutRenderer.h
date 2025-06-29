#pragma once
#include "raylib.h"
#include "clay.h"
#include <array>
#include <vector>

/// @brief A wrapper class that combines the Clay layout engine with a custom raylib renderer for the layout command list.
class LayoutRenderer
{
public:
    LayoutRenderer();
    ~LayoutRenderer();

    void FrameStart();
    void FrameEnd();

    std::array<Font, 1> LoadedFonts;

    Vector2 mousePosition{0,0};
    Vector2 scrollDelta{0,0};
    Clay_Dimensions screenSize{0,0};
    bool screenSizeChanged;

private:
    // std::vector<char> tempRenderBuffer;

    char *temp_render_buffer = nullptr;
    int temp_render_buffer_len = 0;
    Clay_Arena clayMemory;

    void Render(Clay_RenderCommandArray Clay_RenderCommands, Font* fonts);
};