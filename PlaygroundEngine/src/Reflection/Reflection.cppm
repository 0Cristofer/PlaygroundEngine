export module PlaygroundEngine.Reflection;

export import PlaygroundEngine.Reflection.TypeInfo;

export import :Builder;

import std;

namespace PlaygroundEngine
{
	export template <typename T>
	// ReSharper disable once CppUseInternalLinkage
	constexpr const TypeInfo& TypeOf()
	{
		return detail::TypeOfMeta<^^T>();
	}
}
