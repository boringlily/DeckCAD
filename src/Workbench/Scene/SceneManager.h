#pragma once
#include "Scene.h"
#include <vector>

class SceneManager {
public:
    SceneManager() {};

    Scene& GetActiveScene()
    {
        if (sceneList.empty()) {
            CreateEmptyScene();
        }

        return sceneList[activeSceneId];
    };

    void CreateEmptyScene()
    {
        sceneList.emplace_back();
    }

private:
    uint32_t activeSceneId = 0;
    std::vector<Scene> sceneList;
};
