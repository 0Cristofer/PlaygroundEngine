export module PlaygroundEngine.Components.TransformComponent;

import PlaygroundEngine.Components.ComponentBase;

namespace PgE
{
	export class TransformComponent : public ComponentBase
	{
	public:
		void Update() override;

		int Position = 0;
	};
}
