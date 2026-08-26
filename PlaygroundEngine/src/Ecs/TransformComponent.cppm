export module PlaygroundEngine.Ecs.TransformComponent;

import PlaygroundEngine.Ecs.Component;
import PlaygroundEngine.Math;

namespace PgE
{
	// Inherits Transform rather than holding one, so a system writes component->Position and the math type
	// stays usable by the renderer and importers, which have no Component.
	export class TransformComponent : public Component, public Transform
	{};
}
