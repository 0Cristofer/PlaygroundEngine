export module PlaygroundEngine.Ecs.MeshComponent;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.DebugUi.Annotations;

import std;

namespace PgE
{
	// Names a model file under the Models folder beside the executable. A path rather than a handle, so
	// the simulation stays ignorant of renderer resources; the composition root resolves it.
	export class MeshComponent : public Component
	{
	public:
		[[= DrawDebug{}]] std::string MeshPath = "placeholder.obj";
	};
}
