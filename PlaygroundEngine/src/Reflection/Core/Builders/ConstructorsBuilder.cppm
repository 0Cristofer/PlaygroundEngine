module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:ConstructorsBuilder;

import :MetaCommon;
import :FacetsBuilder;
import :AnnotationsBuilder;
import :ArgumentBinding;
import :TypeInfo;
import :ConstructorInfo;
import :ParameterInfo;
import :TypedRef;
import :DeclarationInfo;

import std;

// Builds the ConstructorInfo list for a type. A constructor thunk constructs into the caller's slot with
// placement new, selecting the target constructor by casting the erased arguments to its own parameter types
// (never splicing the constructor, which P2996 forbids). The destroy thunk lives in :DestructorBuilder.

namespace PgE::detail
{
	consteval std::vector<std::meta::info> GetConstructors(const std::meta::info type)
	{
		std::vector<std::meta::info> constructors;
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			if (std::meta::is_constructor(member))
			{
				constructors.push_back(member);
			}
		}
		return constructors;
	}

	consteval ConstructorKind ClassifyConstructor(const std::meta::info constructor)
	{
		if (std::meta::is_default_constructor(constructor))
		{
			return ConstructorKind::Default;
		}
		if (std::meta::is_copy_constructor(constructor))
		{
			return ConstructorKind::Copy;
		}
		if (std::meta::is_move_constructor(constructor))
		{
			return ConstructorKind::Move;
		}
		if (std::meta::parameters_of(constructor).size() == 1)
		{
			return ConstructorKind::Converting;
		}
		return ConstructorKind::Other;
	}

	constexpr ConstructError::Kind ToConstructError(const ArgumentError error)
	{
		// None is not a failure and never reaches here, since CheckConstructArgument returns on it first. It
		// cannot be a precondition: GCC 16 leaves __tu_has_violation undefined for a contract on an inline
		// free function in a .cppm. See docs/ReflectionInternals.md (the argument binder).
		switch (error)
		{
		case ArgumentError::NullArgument:
			return ConstructError::NullArgument;
		case ArgumentError::ConstViolation:
			return ConstructError::ConstViolation;
		case ArgumentError::NotMovable:
			return ConstructError::NotMovable;
		case ArgumentError::TypeMismatch:
		case ArgumentError::None:
			break;
		}

		return ConstructError::TypeMismatch;
	}

	template <std::meta::info MetaParameter>
	void CheckConstructArgument(const TypedRef& arg, const std::size_t index, bool& valid, ConstructError& error)
	{
		// Map the shared neutral check onto ConstructError. A free function (not a lambda) keeps the spliced
		// parameter reflection in a constant-evaluated context (GCC -Wtemplate-body).
		if (!valid)
		{
			return;
		}

		const ArgumentError result = CheckArgument<MetaParameter>(arg);
		if (result == ArgumentError::None)
		{
			return;
		}

		valid = false;
		error = {.Reason = ToConstructError(result), .ArgumentIndex = static_cast<std::uint16_t>(index)};
	}

	template <typename T, std::meta::info MetaConstructor, std::size_t... I>
	void DoConstruct(void* slot, [[maybe_unused]] std::span<const TypedRef> args, std::index_sequence<I...>)
	{
		// Cast each argument to this constructor's own parameter types and construct in place: overload
		// resolution then selects exactly this constructor, no constructor splice needed.
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(MetaConstructor));
		::new (slot) T(ArgumentBinding<parameters[I]>::Bind(args[I])...);
	}

	template <std::meta::info MetaType, std::meta::info MetaConstructor, std::size_t... I>
	std::expected<void, ConstructError> ConstructThunkImpl(std::span<const TypedRef> args, const TypedRef& slot, std::index_sequence<I...>)
	{
		using T = [:MetaType:];
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(MetaConstructor));

		bool valid = true;
		ConstructError error{};
		(CheckConstructArgument<parameters[I]>(args[I], I, valid, error), ...);
		if (!valid)
		{
			return std::unexpected(error);
		}

		// The slot is mandatory, writable storage of exactly the constructed type: construction always
		// produces the object there (there is no "discard" as with a function's return).
		if (slot.Type != &TypeMetaOf<MetaType>() || slot.Data == nullptr || slot.IsConst)
		{
			return std::unexpected(ConstructError{.Reason = ConstructError::SlotTypeMismatch, .ArgumentIndex = 0});
		}

		DoConstruct<T, MetaConstructor>(slot.Data, args, std::index_sequence<I...>{});
		return {};
	}

	template <std::meta::info MetaType, std::meta::info MetaConstructor>
	std::expected<void, ConstructError> ConstructThunk(std::span<const TypedRef> args, const TypedRef& slot)
	{
		constexpr std::size_t parameterCount = std::meta::parameters_of(MetaConstructor).size();
		if (args.size() != parameterCount)
		{
			return std::unexpected(ConstructError{.Reason = ConstructError::ArityMismatch, .ArgumentIndex = 0});
		}

		return ConstructThunkImpl<MetaType, MetaConstructor>(args, slot, std::make_index_sequence<parameterCount>{});
	}

	template <std::meta::info MetaType, std::meta::info MetaConstructor, std::size_t... I>
	consteval bool IsErasedConstructibleImpl(std::index_sequence<I...>)
	{
		// Mirror DoConstruct exactly. A constructor whose placement new is not well-formed on the erased
		// arguments (deleted, or inaccessible: unlike a member function there is no pointer trick that
		// bypasses access for a constructor) reflects as metadata with no thunk.
		using T = [:MetaType:];
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(MetaConstructor));
		return requires(void* slot) { ::new (slot) T(ArgumentBinding<parameters[I]>::Bind(std::declval<const TypedRef&>())...); };
	}

	template <std::meta::info MetaType, std::meta::info MetaConstructor>
	consteval Constructor MakeConstructorThunk()
	{
		if constexpr (!IsImmediateFunction(MetaConstructor) && IsErasedConstructibleImpl<MetaType, MetaConstructor>(
																   std::make_index_sequence<std::meta::parameters_of(MetaConstructor).size()>{}))
		{
			return &ConstructThunk<MetaType, MetaConstructor>;
		}
		else
		{
			return nullptr;
		}
	}

	consteval ConstructorTraits MakeConstructorTraits(const std::meta::info constructor)
	{
		return ConstructorTraits{
			.Access = AccessOf(constructor),
			.Kind = ClassifyConstructor(constructor),
			.IsExplicit = std::meta::is_explicit(constructor),
			.IsDeleted = std::meta::is_deleted(constructor),
			.IsConsteval = IsImmediateFunction(constructor),
			.IsDefaulted = std::meta::is_defaulted(constructor),
		};
	}

	template <std::meta::info MetaType, std::meta::info MetaConstructor>
	consteval ConstructorInfo MakeConstructor(const Constructor construct)
	{
		return ConstructorInfo(MakeParameters<MetaConstructor>(), DisplayStringOf(MetaConstructor), ScopePathOf<MetaConstructor>(),
							   MakeConstructorTraits(MetaConstructor), construct, MakeAnnotations<MetaConstructor>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval std::array<ConstructorInfo, sizeof...(I)> MakeConstructorArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto constructors = std::define_static_array(GetConstructors(MetaType));

		// Thunks left null, set in place on demand (FillConstructorThunks): forming &ConstructThunk odr-uses the
		// constructor, so the metadata build must not, or a deprecated/ill-formed constructor would break it.
		return std::array<ConstructorInfo, sizeof...(I)>{MakeConstructor<MetaType, constructors[I]>(nullptr)...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeConstructorsFromType()
	{
		// A superseding facet views the type through the facet, not its structure, so its constructor list is
		// suppressed for the same reason its fields are.
		if constexpr (ProvidesSupersedingFacet<MetaType>())
		{
			return std::array<ConstructorInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto constructorCount = std::define_static_array(GetConstructors(MetaType)).size();
			return MakeConstructorArray<MetaType>(std::make_index_sequence<constructorCount>{});
		}
		else
		{
			return std::array<ConstructorInfo, 0>{};
		}
	}

	// One mutable array per type; TypeInfo's GetConstructors span points here, and the demand upgrade sets each
	// thunk in place. See the two-tier model in docs/ReflectionInternals.md.
	template <std::meta::info MetaType>
	inline constinit auto GConstructors = MakeConstructorsFromType<MetaType>();

	template <std::meta::info MetaType, std::size_t... I>
	void FillConstructorThunksImpl(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto constructors = std::define_static_array(GetConstructors(MetaType));
		((SetConstructor(GConstructors<MetaType>[I], MakeConstructorThunk<MetaType, constructors[I]>())), ...);
	}

	export template <std::meta::info MetaType>
	void FillConstructorThunks()
	{
		constexpr auto count = std::define_static_array(GetConstructors(MetaType)).size();
		FillConstructorThunksImpl<MetaType>(std::make_index_sequence<count>{});
	}
}
