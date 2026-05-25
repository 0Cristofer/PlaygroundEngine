#pragma once

#include <cassert>
#include <memory>
#include <unordered_map>

#include "PlaygroundEngine/Component/ComponentBase.h"

namespace PlaygroundEngine
{
    class GameObject
    {
        using ComponentId = int;
        
    public:
        void Update();
        
        template <typename T, typename... Args>
        requires(std::is_base_of_v<ComponentBase, T>)
        T* AddComponent(Args&&... args)
        {
            const ComponentId componentId = GetComponentId<T>();
            
            auto [it, added] = _components.try_emplace(componentId, std::make_unique<T>(std::forward<Args>(args)...));
            assert(added && "Added already existing component");
            
            return static_cast<T*>(it->second.get());
        }
        
        template <typename T>
        requires(std::is_base_of_v<ComponentBase, T>)
        [[nodiscard]] T* GetComponent() const
        {
            const ComponentId componentId = GetComponentId<T>();
            
            const auto it = _components.find(componentId);
            assert(it != _components.end() && "GameObject::GetComponent: component not found");
            
            return static_cast<T*>(it->second.get());
        }
        
        template <typename T>
        requires(std::is_base_of_v<ComponentBase, T>)
        [[nodiscard]] bool HasComponent() const
        {
            return _components.contains(GetComponentId<T>());
        }
        
    private:
        static ComponentId IncrementComponentId();

        template <typename T>
        requires(std::is_base_of_v<ComponentBase, T>)
        [[nodiscard]] static ComponentId GetComponentId()
        {
            static const ComponentId componentId = IncrementComponentId();
            
            return componentId;
        }
        
        std::unordered_map<ComponentId, std::unique_ptr<ComponentBase>> _components;
    };   
}
