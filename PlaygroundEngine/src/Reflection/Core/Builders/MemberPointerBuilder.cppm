module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:MemberPointerBuilder;

import :MetaCommon;
import :MemberPointerInfo;

import std;

// Builds the MemberPointerInfo for a pointer-to-member type. std::meta has no remove_member_pointer and no
// member_pointer_class_of, so the two halves come from a partial specialization on the type itself.
// See docs/ReflectionExtraction.md (member pointers).

namespace PgE::detail
{
	template <typename Pointer>
	struct MemberPointerParts;

	template <typename Class, typename Pointee>
	struct MemberPointerParts<Pointee Class::*>
	{
		using ClassType = Class;
		using PointeeType = Pointee;
	};

	template <std::meta::info MetaType>
	constexpr const MemberPointerInfo& MemberPointerMetaOf()
	{
		// The aliases keep a spliced type and a dependent name out of the template arguments below, neither of
		// which GCC accepts there (-Wtemplate-body).
		using Pointer = [:MetaType:];
		using Parts = MemberPointerParts<Pointer>;
		using ClassType = typename Parts::ClassType;
		using PointeeType = typename Parts::PointeeType;

		static constexpr MemberPointerInfo MemberPointer(TypeReferenceTo<^^ClassType>(), TypeReferenceTo<^^PointeeType>());
		return MemberPointer;
	}

	template <std::meta::info MetaType>
	consteval const MemberPointerInfo* MakeMemberPointer()
	{
		if constexpr (std::meta::is_member_object_pointer_type(MetaType) || std::meta::is_member_function_pointer_type(MetaType))
		{
			return &MemberPointerMetaOf<MetaType>();
		}
		else
		{
			return nullptr;
		}
	}
}
