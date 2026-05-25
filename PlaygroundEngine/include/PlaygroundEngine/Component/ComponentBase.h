#pragma once

namespace PlaygroundEngine
{
    class ComponentBase
    {
    public:
        virtual void Update() = 0;
        
        virtual ~ComponentBase() = default;
    };   
}
