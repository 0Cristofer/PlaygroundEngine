export module PlaygroundEngine.Reflection.Builtins:StringFacetBuilder;

import PlaygroundEngine.Reflection.Core;

import :StringFacet;

import std;

// The pre-made string facet and its TypeInfoTraits specializations. A char-based std::string or
// std::string_view exposes a StringFacet.

namespace PgE
{
	namespace detail
	{
		template <typename S>
		std::string_view StringViewThunk(const void* obj)
		{
			return std::string_view{*static_cast<const S*>(obj)};
		}

		template <typename S>
		std::expected<void, FacetError> StringAssignThunk(void* obj, const std::string_view value)
		{
			*static_cast<S*>(obj) = value;
			return {};
		}
	}

	template <typename Traits, typename Alloc>
	struct TypeInfoTraits<std::basic_string<char, Traits, Alloc>> : TypeInfoTraitsDefaults
	{
		// char-based strings only: StringFacet::View yields a std::string_view, which a wide string cannot
		// produce. A non-char string reflects structurally (no case for one exists in reflected data yet).

		using String = std::basic_string<char, Traits, Alloc>;

		static std::string Stringify(const String& obj) { return std::format("\"{}\"", obj); }

		static consteval auto MakeFacets() { return std::tuple{StringFacet{&detail::StringViewThunk<String>, &detail::StringAssignThunk<String>}}; }
	};

	template <typename Traits>
	struct TypeInfoTraits<std::basic_string_view<char, Traits>> : TypeInfoTraitsDefaults
	{
		using View = std::basic_string_view<char, Traits>;

		static std::string Stringify(const View& obj) { return std::format("\"{}\"", obj); }

		// A view reads but never writes: the assign thunk is null, the same nullable-capability encoding
		// fields already use.
		static consteval auto MakeFacets() { return std::tuple{StringFacet{&detail::StringViewThunk<View>, nullptr}}; }
	};
}
