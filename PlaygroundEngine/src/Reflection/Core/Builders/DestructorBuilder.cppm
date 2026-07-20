module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:DestructorBuilder;

import :MetaCommon;
import :AnnotationsBuilder;
import :DestructorInfo;
import :DeclarationInfo;

import std;

// Builds a type's single DestructorInfo: the lifetime-end metadata read off the destructor member and the
// type-scoped destroy thunk. A type with no reachable destructor reflects one with a null thunk. See
// docs/ReflectionInternals.md (lifetime ops).

namespace PgE::detail
{
	template <typename T>
	void DestroyThunk(void* obj)
	{
		static_cast<T*>(obj)->~T();
	}

	template <std::meta::info MetaType>
	consteval DestructorInfo::Destroyer MakeFullDestroyer()
	{
		// obj->~T() reaches a real/pseudo destructor only for a scalar or class object; is_destructible excludes
		// a deleted or inaccessible one, and arrays are excluded because ~T() is ill-formed on them (elements
		// are destroyed individually). Anything else reflects with no destroy thunk.
		using T = [:MetaType:];

		// Nested, not one && chain: is_destructible_v requires a complete type, and a variable template is
		// instantiated even where && would short-circuit its evaluation.
		if constexpr (std::meta::is_complete_type(MetaType))
		{
			if constexpr (std::is_object_v<T> && std::is_destructible_v<T> && !std::is_array_v<T>)
			{
				return &DestroyThunk<T>;
			}
			else
			{
				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}
	}

	template <std::meta::info MetaType>
	consteval DestructorInfo::Destroyer MakeTrivialDestroyer()
	{
		// The transitive-safe half: a trivial ~T has no body to instantiate, so its thunk is formed during the
		// metadata build like a field offset. A non-trivial destructor is left null and filled on demand, so a
		// reached type never odr-uses a destructor body that could be ill-formed for this instantiation.
		using T = [:MetaType:];

		// Nested, not one && chain: is_trivially_destructible_v requires a complete type, and a variable template
		// is instantiated even where && would short-circuit, so an incomplete handle would ill-form the query.
		if constexpr (std::meta::is_complete_type(MetaType))
		{
			if constexpr (std::is_object_v<T> && std::is_trivially_destructible_v<T> && !std::is_array_v<T>)
			{
				return &DestroyThunk<T>;
			}
			else
			{
				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}
	}

	consteval std::optional<std::meta::info> FindDestructor(const std::meta::info type)
	{
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			if (std::meta::is_destructor(member))
			{
				return member;
			}
		}
		return std::nullopt;
	}

	consteval DestructorTraits MakeDestructorTraits(const std::meta::info type, const std::optional<std::meta::info> destructor)
	{
		DestructorTraits traits{};

		// Trivially-destructible is a whole-type answer, defined only for a complete object type; a reference,
		// void, or opaque handle leaves it at its default.
		if (std::meta::is_complete_type(type) && std::meta::is_object_type(type))
		{
			traits.IsTrivial = std::meta::is_trivially_destructible_type(type);
		}

		// The per-declaration facts live on the destructor member, which only a class or union has. A scalar
		// keeps the defaults: no member is defaulted or virtual, and IsTrivial above already answered triviality.
		if (destructor.has_value())
		{
			traits.Access = AccessOf(*destructor);
			traits.IsVirtual = std::meta::is_virtual(*destructor);
			traits.IsPureVirtual = std::meta::is_pure_virtual(*destructor);
			traits.IsDeleted = std::meta::is_deleted(*destructor);
			traits.IsDefaulted = std::meta::is_defaulted(*destructor);
		}

		return traits;
	}

	template <std::meta::info MetaType>
	consteval DestructorInfo MakeDestructor()
	{
		// The member walk runs once and its result feeds both the traits and the naming, so the two cannot
		// disagree about whether this type has a destructor member to read.
		if constexpr (IsClassOrUnion(MetaType))
		{
			if constexpr (constexpr std::optional<std::meta::info> destructor = FindDestructor(MetaType); destructor.has_value())
			{
				constexpr std::meta::info member = *destructor;
				return DestructorInfo(DisplayStringOf(member), ScopePathOf<member>(), MakeDestructorTraits(MetaType, destructor),
									  MakeTrivialDestroyer<MetaType>(), MakeAnnotations<member>());
			}
			else
			{
				return DestructorInfo({}, {}, MakeDestructorTraits(MetaType, destructor), MakeTrivialDestroyer<MetaType>(), {});
			}
		}
		else
		{
			return DestructorInfo({}, {}, MakeDestructorTraits(MetaType, std::nullopt), MakeTrivialDestroyer<MetaType>(), {});
		}
	}

	// One mutable DestructorInfo per type; TypeInfo's GetDestructor points here. It carries the trivial destroyer
	// from the metadata build, and the demand upgrade sets a non-trivial one in place. See the two-tier model.
	template <std::meta::info MetaType>
	inline constinit DestructorInfo GDestructor = MakeDestructor<MetaType>();

	export template <std::meta::info MetaType>
	void FillDestroyer()
	{
		SetDestroyer(GDestructor<MetaType>, MakeFullDestroyer<MetaType>());
	}
}
