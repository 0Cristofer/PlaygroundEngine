import PlaygroundEngine;
import PlaygroundEngine.Components;

// Only needed for GCC, since it doesn't export operator new correctly with template instantiations. Remove this to see the
import std;

int main(int, char**)
{
    PlaygroundEngine::Engine engine;

    PlaygroundEngine::World* world = engine.GetWorld();

    PlaygroundEngine::GameObject* gO1 = world->AddGameObject();
    PlaygroundEngine::TransformComponent* component = gO1->AddComponent<PlaygroundEngine::TransformComponent>();

    component->Position = 3;
    engine.Run();

    gO1->GetComponent<PlaygroundEngine::TransformComponent>()->Position = 2;
    engine.Run();
    
    return 0;
}
