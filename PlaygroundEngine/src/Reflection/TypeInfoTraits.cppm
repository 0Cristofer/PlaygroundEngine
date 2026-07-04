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
		static constexpr bool IsLeaf = false;

		template <typename T>
		static std::string Stringify(const T& obj)
		{
			// Every type stays reflectable: a leaf with no formatter or trait falls back to its reflected
			// type name, so it renders as a placeholder rather than failing to compile.
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

	template <typename CharT, typename Traits, typename Alloc>
	struct TypeInfoTraits<std::basic_string<CharT, Traits, Alloc>> : TypeInfoTraitsDefaults
	{
		static constexpr bool IsLeaf = true;

		static std::string Stringify(const std::string& obj)
		{
			return std::format("\"{}\"", obj);
		}
	};

	template<typename T>
	struct TypeInfoTraits<T*> : TypeInfoTraitsDefaults
	{
		static std::string Stringify(const T* obj)
		{
			return std::format("{}", static_cast<const void*>(obj));
		}
	};
}
