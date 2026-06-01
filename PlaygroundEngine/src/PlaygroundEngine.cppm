export module PlaygroundEngine;

export import PlaygroundEngine.World;
export import PlaygroundEngine.GameObject;

import std;

namespace PlaygroundEngine
{
    export class Engine
    {
    public:
        Engine();

        [[nodiscard]] World* GetWorld() const;
        void Run();
        
    private:
        std::unique_ptr<World> _currentWorld;
    };   
}
