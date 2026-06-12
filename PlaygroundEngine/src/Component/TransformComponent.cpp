module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine.Components.TransformComponent;

import PlaygroundEngine.Log;

void PlaygroundEngine::TransformComponent::Update()
{
	LOG_TRACE("Updating Transform with position: {0}", Position);
}
