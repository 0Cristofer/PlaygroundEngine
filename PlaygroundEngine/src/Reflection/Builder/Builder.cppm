module;

#include <meta>

export module PlaygroundEngine.Reflection:Builder;

import PlaygroundEngine.Reflection.TypeInfoTraits;

export import :BuilderCommon;
export import :BuilderAnnotations;
export import :BuilderFields;
export import :BuilderFunctions;
import :TypeInfo;

import std;

// Assembles the runtime TypeInfo for a type and holds the sole definition of the TypeOfMeta recursion
// knot (declared in :BuilderCommon). Instantiating it here pulls in the field and function builders,
// which recurse back into TypeOfMeta for member and signature types; the definition being visible only
// in this partition is what keeps the sub-builders dependent on the declaration alone.

namespace PgE::detail
{
	template <std::meta::info MetaType>
	constexpr const TypeInfo& TypeOfMeta()
	{
		using T = [:MetaType:];
		constexpr std::string_view identifier = IdentifierOf(MetaType);
		constexpr std::string_view displayName = DisplayStringOf(MetaType);

		static constexpr auto Fields = MakeFieldsFromType<MetaType>();
		static constexpr auto Functions = MakeFunctionsFromType<MetaType>();

		static constexpr auto Annotations = MakeAnnotations<MetaType>();

		// std::is_object_v excludes void (reached here as a function return type), where
		// StringifyValue can't form a const T*; it stays true for primitives and classes.
		if constexpr (Fields.empty() && std::is_object_v<T>)
		{
			static constexpr TypeInfo Result(identifier, displayName, Fields, Functions, &StringifyValue<T>,
			                                 Annotations);
			return Result;
		}
		else
		{
			static constexpr TypeInfo Result(identifier, displayName, Fields, Functions, nullptr, Annotations);
			return Result;
		}
	}
}