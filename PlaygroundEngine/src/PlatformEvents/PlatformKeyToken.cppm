export module PlaygroundEngine.PlatformEvents:PlatformKeyToken;

import std;

namespace PgE
{
	/// Opaque, meaningful only to the backend that produced it, and the only thing a layout-dependent
	/// display name can be derived from. A strong type so it cannot be mistaken for an InputCode.
	export struct PlatformKeyToken
	{
		std::int32_t Value = 0;
	};
}
