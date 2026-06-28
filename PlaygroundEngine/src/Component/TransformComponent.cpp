module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine.Components.TransformComponent;

import PlaygroundEngine.Log;

void PgE::TransformComponent::Update()
{
	PGE_LOG(Trace, "Updating Transform with position: {0}", Position);
}
