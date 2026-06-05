module;

#include <PlaygroundEngine/Log.h>

module PlaygroundGame;

import PlaygroundEngine.Components.TransformComponent;
import PlaygroundEngine.Log;

std::unique_ptr<PlaygroundEngine::AppBase> PlaygroundGame::PlaygroundGameAppDescriptor::GetApp()
{
    return std::make_unique<App>();
}

void PlaygroundGame::App::OnInitialized()
{
    LOG_INFO("PlaygroundGame::App::OnInitialized");
}

void PlaygroundGame::App::OnRun(PlaygroundEngine::World* world)
{
    static int a = 2;
    PlaygroundEngine::GameObject* gO1 = world->AddGameObject();
    PlaygroundEngine::TransformComponent* component = gO1->AddComponent<PlaygroundEngine::TransformComponent>();
    component->Position = a;
    LOG_INFO("PlaygroundGame::App::OnRun {}", component->Position);
    a++;
}
