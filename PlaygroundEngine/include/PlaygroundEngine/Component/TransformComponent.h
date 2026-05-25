#pragma once

#include "PlaygroundEngine/Component/ComponentBase.h"

namespace PlaygroundEngine
{
    class TransformComponent : public ComponentBase
    {
    public:
        void Update() override;

        int Position = 0;
    };   
}
