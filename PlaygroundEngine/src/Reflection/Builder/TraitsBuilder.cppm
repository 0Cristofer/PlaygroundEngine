module;

#include <meta>

export module PlaygroundEngine.Reflection:TraitsBuilder;

import :TypeInfo;

import std;

// Fills TypeTraits from the type system: category, byte layout, and the trait predicates that
// downstream systems (serialization, C# marshaling, GPU interop, replication) branch on. Each fact
// is a whole-type scalar read straight off the std::meta::info, with no recursion into members.
// Non-std::meta providers (runtime-constructed types, generated code) populate the same struct.

namespace PgE::detail
{
	template <std::meta::info Type>
	consteval TypeKind KindOf()
	{
		if constexpr (std::meta::is_void_type(Type))
			return TypeKind::Void;
		else if constexpr (std::meta::is_null_pointer_type(Type))
			return TypeKind::NullPointer;
		else if constexpr (std::meta::is_integral_type(Type))
			return TypeKind::Integral;
		else if constexpr (std::meta::is_floating_point_type(Type))
			return TypeKind::FloatingPoint;
		else if constexpr (std::meta::is_enum_type(Type))
			return TypeKind::Enum;
		else if constexpr (std::meta::is_union_type(Type))
			return TypeKind::Union;
		else if constexpr (std::meta::is_class_type(Type))
			return TypeKind::Class;
		else if constexpr (std::meta::is_array_type(Type))
			return TypeKind::Array;
		else if constexpr (std::meta::is_pointer_type(Type))
			return TypeKind::Pointer;
		else if constexpr (std::meta::is_member_object_pointer_type(Type))
			return TypeKind::MemberObjectPointer;
		else if constexpr (std::meta::is_member_function_pointer_type(Type))
			return TypeKind::MemberFunctionPointer;
		else if constexpr (std::meta::is_function_type(Type))
			return TypeKind::Function;
		else if constexpr (std::meta::is_lvalue_reference_type(Type))
			return TypeKind::LValueReference;
		else if constexpr (std::meta::is_rvalue_reference_type(Type))
			return TypeKind::RValueReference;
		else
			return TypeKind::Other;
	}

	template <std::meta::info Type>
	consteval std::size_t SizeOf()
	{
		if constexpr (std::meta::is_object_type(Type))
			return std::meta::size_of(Type);
		else
			return 0;
	}

	template <std::meta::info Type>
	consteval std::size_t AlignmentOf()
	{
		if constexpr (std::meta::is_object_type(Type))
			return std::meta::alignment_of(Type);
		else
			return 0;
	}

	template <std::meta::info Type>
	consteval TypeTraits MakeTraits()
	{
		return TypeTraits{
			.Kind = KindOf<Type>(),
			.Size = SizeOf<Type>(),
			.Alignment = AlignmentOf<Type>(),
			.IsTriviallyCopyable = std::meta::is_trivially_copyable_type(Type),
			.IsTriviallyDefaultConstructible = std::meta::is_trivially_default_constructible_type(Type),
			.IsTriviallyDestructible = std::meta::is_trivially_destructible_type(Type),
			.IsStandardLayout = std::meta::is_standard_layout_type(Type),
			.HasUniqueObjectRepresentations = std::meta::has_unique_object_representations(Type),
			.IsDefaultConstructible = std::meta::is_default_constructible_type(Type),
			.IsAggregate = std::meta::is_aggregate_type(Type),
			.IsPolymorphic = std::meta::is_polymorphic_type(Type),
			.IsAbstract = std::meta::is_abstract_type(Type),
			.HasVirtualDestructor = std::meta::has_virtual_destructor(Type),
			.IsEmpty = std::meta::is_empty_type(Type),
			.IsTemplateInstance = std::meta::has_template_arguments(Type),
			.IsSigned = std::meta::is_signed_type(Type),
			.IsScopedEnum = std::meta::is_scoped_enum_type(Type),
		};
	}
}
