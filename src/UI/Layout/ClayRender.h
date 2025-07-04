#pragma once
#include "raylib.h"
#include "clay.h"
#include <array>
#include <vector>
#include "assert.h"

namespace UI
{
    /// @brief A wrapper class that combines the Clay layout engine with a custom raylib renderer for the layout command list.
    class ClayRender
    {
    public:
        ClayRender();
        ~ClayRender();
        
        void StartFrame();
        void EndFrame();

    private:
    
        std::array<Font, 1> LoadedFonts;
        Vector2 mousePosition{0,0};
        Vector2 scrollDelta{0,0};
        Clay_Dimensions screenSize{0,0};
        bool screenSizeChanged;

        std::vector<char> tempRenderBuffer;

        void Render(Clay_RenderCommandArray Clay_RenderCommands, Font* fonts);
    };
};