#pragma once
#include "Scene/Scene.h"

#include <vector>
#include <optional>
#include "assert.h"

enum class AppLayer {
    Home_Layer,
    Settings_Layer,
    Scene_Layer
};

using SceneList = std::vector<Scene>;

class AppMemory {
public:
    // Global Data

    void CreateNewScene()
    {
        u32 new_scene_id { static_cast<u32>(scenes.size()) };
        scenes.push_back(Scene { std::format("Untitled {}", new_scene_id), canvas_texture });
        active_scene_id = new_scene_id;
        active_layer = AppLayer::Scene_Layer;
    }

    [[nodiscard]] bool TryActivateScene(u32 scene_id)
    {
        if (scene_id < scenes.size()) {
            active_scene_id = scene_id;
            return true;
        }

        return false;
    }

    [[nodiscard]] Scene* GetActiveScene()
    {
        if (active_layer != AppLayer::Scene_Layer || active_scene_id >= scenes.size()) {
            active_scene_id = 0;
            active_layer = AppLayer::Home_Layer;

            return nullptr;
        }

        return &scenes.at(active_scene_id);
    }

    [[nodiscard]] SceneList& GetSceneList()
    {
        return scenes;
    }

    [[nodiscard]] bool IsHomeLayerActive()
    {
        return active_layer == AppLayer::Home_Layer;
    }

    [[nodiscard]] bool IsSceneLayerActive()
    {
        return active_layer == AppLayer::Scene_Layer;
    }

    [[nodiscard]] bool IsSettingsLayerActive()
    {
        return active_layer == AppLayer::Settings_Layer;
    }

    [[nodiscard]] AppLayer GetActiveLayer()
    {
        return active_layer;
    }

    void ActivateHomeLayer()
    {
        active_layer = AppLayer::Home_Layer;
    }

    void ActivateSceneLayer()
    {
        active_layer = AppLayer::Scene_Layer;
    }

    void ActivateSettingsLayer()
    {
        active_layer = AppLayer::Settings_Layer;
    }

    [[nodiscard]] s32 GetActiveSceneId()
    {
        return active_scene_id;
    }

private:
    s32 active_scene_id { -1 };

    AppLayer active_layer { AppLayer::Home_Layer };

    RenderTexture canvas_texture;
    SceneList scenes {};
};
