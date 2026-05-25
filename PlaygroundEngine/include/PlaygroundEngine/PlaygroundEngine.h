#pragma once

#include <memory>

#define SPDLOG_HEADER_ONLY

namespace PlaygroundEngine
{
    class World;

    class Engine
    {
    public:
        Engine();

        [[nodiscard]] World* GetWorld() const;
        void Run();
        
    private:
        std::unique_ptr<World> _currentWorld;
    };   
}
