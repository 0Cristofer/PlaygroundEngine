#include "PlaygroundEngine/Component/TransformComponent.h"

#include "PlaygroundEngine/Log.h"

void PlaygroundEngine::TransformComponent::Update()
{
    LOG_TRACE("Updating Transform with position: {0}", Position);
}
