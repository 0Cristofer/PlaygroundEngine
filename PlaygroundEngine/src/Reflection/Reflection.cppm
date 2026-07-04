export module PlaygroundEngine.Reflection;

export import :TypeInfo;
export import :FieldInfo;
export import :FuncInfo;
export import :TypedRef;
export import :Annotation;
export import :Builder;

import std;

namespace PgE
{
	export template <typename T>
	constexpr const TypeInfo& TypeOf()
	{
		return detail::TypeOfMeta<^^T>();
	}

	export template <typename T>
	std::string ToString(const T& value)
	{
		return TypeOf<T>().ObjectToString(&value);
	}
}
