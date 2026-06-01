export module PlaygroundEngine.Components.ComponentBase;

namespace PlaygroundEngine
{
    export class ComponentBase
    {
    public:
        virtual void Update() = 0;
        
        virtual ~ComponentBase() = default;
    };   
}
