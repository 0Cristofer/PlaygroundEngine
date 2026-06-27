export module PlaygroundEngine.Reflection;

export import PlaygroundEngine.Reflection.TypeInfo;

export import :Builder;

import std;

namespace PgE
{
	export template <typename T>
	constexpr const TypeInfo& TypeOf()
	{
		return detail::TypeOfMeta<^^T>();
	}
}
