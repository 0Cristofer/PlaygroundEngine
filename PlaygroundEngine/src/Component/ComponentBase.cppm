export module PlaygroundEngine.Components.ComponentBase;

namespace PgE
{
	export class ComponentBase
	{
	public:
		virtual void Update() = 0;

		virtual ~ComponentBase() = default;
	};
}
