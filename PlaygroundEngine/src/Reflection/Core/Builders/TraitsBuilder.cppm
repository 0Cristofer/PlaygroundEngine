module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:TraitsBuilder;

import :TypeInfo;

import std;

// Fills TypeTraits from the type system: category, byte layout, and the trait predicates downstream
// systems (serialization, C# marshaling, GPU interop, replication) branch on. Each fact is a whole-type
// scalar read off the std::meta::info; non-std::meta providers populate the same struct.

namespace PgE::detail
{
	template <std::meta::info Type>
	consteval TypeKind KindOf()
	{
		if constexpr (std::meta::is_void_type(Type))
		{
			return TypeKind::Void;
		}
		else if constexpr (std::meta::is_null_pointer_type(Type))
		{
			return TypeKind::NullPointer;
		}
		else if constexpr (std::meta::is_integral_type(Type))
		{
			return TypeKind::Integral;
		}
		else if constexpr (std::meta::is_floating_point_type(Type))
		{
			return TypeKind::FloatingPoint;
		}
		else if constexpr (std::meta::is_enum_type(Type))
		{
			return TypeKind::Enum;
		}
		else if constexpr (std::meta::is_union_type(Type))
		{
			return TypeKind::Union;
		}
		else if constexpr (std::meta::is_class_type(Type))
		{
			return TypeKind::Class;
		}
		else if constexpr (std::meta::is_array_type(Type))
		{
			return TypeKind::Array;
		}
		else if constexpr (std::meta::is_pointer_type(Type))
		{
			return TypeKind::Pointer;
		}
		else if constexpr (std::meta::is_member_object_pointer_type(Type))
		{
			return TypeKind::MemberObjectPointer;
		}
		else if constexpr (std::meta::is_member_function_pointer_type(Type))
		{
			return TypeKind::MemberFunctionPointer;
		}
		else if constexpr (std::meta::is_function_type(Type))
		{
			return TypeKind::Function;
		}
		else if constexpr (std::meta::is_lvalue_reference_type(Type))
		{
			return TypeKind::LValueReference;
		}
		else if constexpr (std::meta::is_rvalue_reference_type(Type))
		{
			return TypeKind::RValueReference;
		}
		else
		{
			return TypeKind::Other;
		}
	}

	consteval LinkageKind LinkageOf(const std::meta::info type)
	{
		if (std::meta::has_external_linkage(type))
		{
			return LinkageKind::External;
		}
		if (std::meta::has_module_linkage(type))
		{
			return LinkageKind::Module;
		}
		if (std::meta::has_internal_linkage(type))
		{
			return LinkageKind::Internal;
		}
		return LinkageKind::None;
	}

	template <std::meta::info Type>
	consteval std::size_t SizeOf()
	{
		// Completeness is the guard, and it covers more than it looks: an incomplete class (an opaque handle
		// named by a pointer), an unbounded array, and void are all sizeless, and size_of is ill-formed for
		// each. See docs/ReflectionInternals.md (incomplete types).
		if constexpr (std::meta::is_object_type(Type) && std::meta::is_complete_type(Type))
		{
			return std::meta::size_of(Type);
		}
		else
		{
			return 0;
		}
	}

	template <std::meta::info Type>
	consteval std::size_t ExtentOf()
	{
		if constexpr (std::meta::is_array_type(Type))
		{
			return std::meta::extent(Type);
		}
		else
		{
			return 0;
		}
	}

	template <std::meta::info Type>
	consteval std::size_t AlignmentOf()
	{
		if constexpr (std::meta::is_object_type(Type) && std::meta::is_complete_type(Type))
		{
			return std::meta::alignment_of(Type);
		}
		else
		{
			return 0;
		}
	}

	template <std::meta::info Type>
	consteval TypeTraits MakeTraits()
	{
		// The facts that hold for any type, complete or not: its category, how it is named and linked, and
		// the shape it decomposes into.
		TypeTraits traits{
			.Kind = KindOf<Type>(),
			.Linkage = LinkageOf(Type),
			.Size = SizeOf<Type>(),
			.Alignment = AlignmentOf<Type>(),
			.IsTemplateInstance = std::meta::has_template_arguments(Type),
			.IsConst = std::meta::is_const_type(Type),
			.IsVolatile = std::meta::is_volatile_type(Type),
			.Extent = ExtentOf<Type>(),
		};

		// Every predicate below has a complete-type precondition, and an incomplete type genuinely has no
		// answer for them: its definition is what decides them. They stay at their defaults, which is what an
		// opaque handle reflects as. See docs/ReflectionInternals.md (incomplete types).
		if constexpr (std::meta::is_complete_type(Type))
		{
			traits.IsTriviallyCopyable = std::meta::is_trivially_copyable_type(Type);
			traits.IsTriviallyDefaultConstructible = std::meta::is_trivially_default_constructible_type(Type);
			traits.IsTriviallyDestructible = std::meta::is_trivially_destructible_type(Type);
			traits.IsStandardLayout = std::meta::is_standard_layout_type(Type);
			traits.HasUniqueObjectRepresentations = std::meta::has_unique_object_representations(Type);
			traits.IsDefaultConstructible = std::meta::is_default_constructible_type(Type);
			traits.IsAggregate = std::meta::is_aggregate_type(Type);
			traits.IsPolymorphic = std::meta::is_polymorphic_type(Type);
			traits.IsAbstract = std::meta::is_abstract_type(Type);
			traits.HasVirtualDestructor = std::meta::has_virtual_destructor(Type);
			traits.IsEmpty = std::meta::is_empty_type(Type);
			traits.IsFinal = std::meta::is_final(Type);
			traits.IsSigned = std::meta::is_signed_type(Type);
			traits.IsScopedEnum = std::meta::is_scoped_enum_type(Type);
		}

		return traits;
	}
}
