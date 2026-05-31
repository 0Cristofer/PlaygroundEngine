#include "PlaygroundEngine/World.h"

import Test;

PlaygroundEngine::GameObject* PlaygroundEngine::World::AddGameObject()
{
    _gameObjects.push_back(std::make_unique<GameObject>());
    Test t;
    t.GetA();
    
    return _gameObjects.back().get();
}

void PlaygroundEngine::World::Run()
{
    for (auto& gameObject : _gameObjects)
    {
        gameObject->Update();
    }
}
