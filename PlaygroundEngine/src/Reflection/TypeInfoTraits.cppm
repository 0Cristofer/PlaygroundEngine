module;

#include <meta>

export module PlaygroundEngine.Reflection.TypeInfoTraits;

import std;

namespace PgE
{
	namespace detail
	{
		template <typename T>
		consteval std::string_view TypeName()
		{
			return std::define_static_string(std::meta::display_string_of(^^T));
		}
	}

	export struct TypeInfoTraitsDefaults
	{
		// Stringifying is total: a leaf renders through its formatter, and a type with no formatter falls
		// back to its own reflected type name. So "what is this as a string" is answered entirely here, and
		// the renderer never has to special-case a type that lacks a textual form.
		template <typename T>
		static std::string Stringify(const T& obj)
		{
			if constexpr (std::formattable<T, char>)
				return std::format("{}", obj);
			else
				return std::format("<{}>", detail::TypeName<T>());
		}
	};

	export template <typename>
	struct TypeInfoTraits : TypeInfoTraitsDefaults
	{
	};

	template <typename T>
	struct TypeInfoTraits<T*> : TypeInfoTraitsDefaults
	{
		static std::string Stringify(const T* obj)
		{
			// Only a pointer to an object can be cast to const void* for its address; a function pointer
			// cannot, so it renders as a placeholder rather than its (not very useful) numeric value.
			if constexpr (std::is_object_v<T>)
				return std::format("{}", static_cast<const void*>(obj));
			else
				return obj ? "<function>" : "<null>";
		}
	};
}
