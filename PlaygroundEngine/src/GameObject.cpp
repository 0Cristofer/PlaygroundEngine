#include "PlaygroundEngine/GameObject.h"

#include <ranges>

namespace PlaygroundEngine
{
    void GameObject::Update()
    {
        for (const auto& component : _components | std::views::values)
        {
            component->Update();
        }
    }

    GameObject::ComponentId GameObject::IncrementComponentId()
    {
        static ComponentId id = 0;
        return id++;
    }
}
