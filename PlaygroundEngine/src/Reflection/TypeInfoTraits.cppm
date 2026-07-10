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

	template <typename T>
		requires std::is_enum_v<T>
	struct TypeInfoTraits<T> : TypeInfoTraitsDefaults
	{
		// An enum is a leaf that renders as its enumerator name; a value no enumerator names (a flag
		// combination, or a static_cast in-range value) falls back to its number. The match is on the
		// underlying integer so aliased and zero-valued enumerators compare correctly.
		static constexpr bool IsLeaf = true;

		static std::string Stringify(const T value)
		{
			using Underlying = std::underlying_type_t<T>;
			const Underlying raw = static_cast<Underlying>(value);

			template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^T)))
			{
				constexpr Underlying enumeratorValue = static_cast<Underlying>([:enumerator:]);
				constexpr std::string_view name = std::define_static_string(std::meta::identifier_of(enumerator));
				if (raw == enumeratorValue)
					return std::string(name);
			}

			if constexpr (std::is_signed_v<Underlying>)
				return std::format("{}", static_cast<std::int64_t>(raw));
			else
				return std::format("{}", static_cast<std::uint64_t>(raw));
		}
	};
}
