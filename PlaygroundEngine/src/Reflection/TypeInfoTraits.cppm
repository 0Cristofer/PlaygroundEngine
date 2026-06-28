export module PlaygroundEngine.Reflection.TypeInfoTraits;

import std;

namespace PgE
{
	export struct TypeInfoTraitsDefaults
	{
		static constexpr bool IS_LEAF = false;

		template <typename T>
		static std::string Stringify(const T& obj)
		{
			if constexpr (std::formattable<T, char>)
				return std::format("{}", obj);
			else
				static_assert(false, "Leaf type needs a TypeInfoTraits<T>::Stringify specialization");

			return "";
		}
	};

	export template <typename>
	struct TypeInfoTraits : TypeInfoTraitsDefaults
	{
	};

	template <typename CharT, typename Traits, typename Alloc>
	struct TypeInfoTraits<std::basic_string<CharT, Traits, Alloc>> : TypeInfoTraitsDefaults
	{
		static constexpr bool IS_LEAF = true;

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
