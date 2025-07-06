#pragma once
#include "Component.h"
#include "ClayPrimitives.h"
#include "CanvasCamera.h"
#include "raylib.h"

class CanvasComponent : public UI::Component {
public:
    // Only delete the default constructor;
    CanvasComponent() = delete;

    CanvasComponent(Model& model)
        : Component("Canvas")
        , exampleModel(model)
    {
        static constexpr float CANVAS_WIDTH_SHRINK_MIN { 500.0f };

        elementConfig = {
            .id = clayId,
            .layout = {
                .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(CANVAS_WIDTH_SHRINK_MIN),
            },
            .image = { .imageData = &canvasTexture.texture },
        };
    };

protected:
    void UpdateInternal() override;

private:
    Model& exampleModel;

    CanvasCamera camera;

    RenderTexture2D canvasTexture;
};