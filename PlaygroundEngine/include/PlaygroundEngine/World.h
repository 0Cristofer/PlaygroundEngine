#pragma once

#include <memory>
#include <vector>

#include "PlaygroundEngine/GameObject.h"

namespace PlaygroundEngine
{
    class World
    {
    public:
        GameObject* AddGameObject();
        void Run();
    
    private:
        std::vector<std::unique_ptr<GameObject>> _gameObjects;
    };   
}
