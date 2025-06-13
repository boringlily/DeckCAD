#pragma once
#include "Scene.h"

class SceneManager
{
    public:
    SceneManager()
    {
        active_scene = {};
    };

    static Scene &GetActiveScene()
    {
        return active_scene;
    };

    private: 

    static Scene active_scene;
};

