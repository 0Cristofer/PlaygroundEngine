module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:StaticFieldsBuilder;

import :MetaCommon;
import :FacetsBuilder;
import :AnnotationsBuilder;
import :TypeInfo;
import :FieldInfo;
import :StaticFieldInfo;
import :TypedRef;
import :DeclarationInfo;

import std;

// Builds the StaticFieldInfo list for a type. The whole shape of this builder is the value/address split:
// a constant-readable member is captured by value and never odr-used, everything else takes an address.
// See docs/ReflectionInternals.md (static data members).

namespace PgE::detail
{
	template <auto Value>
	struct ConstantHolder
	{};

	template <std::meta::info MetaField>
	concept ConstantReadable = requires { ConstantHolder<([:MetaField:])>{}; };

	template <std::meta::info MetaField>
	struct StaticFieldValue
	{
		// The captured value. Binding the splice to a constexpr initializer is a constant read, not an
		// odr-use, so a static const with no out-of-line definition never reaches the linker.
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		static constexpr Field Value = [:MetaField:];
	};

	template <std::meta::info MetaField>
	std::expected<void, FieldError> StaticFieldValueGetThunk(const TypedRef& out)
	{
		using Field = typename StaticFieldValue<MetaField>::Field;

		if (out.Type != &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaField))>())
		{
			return std::unexpected(FieldError{FieldError::TypeMismatch});
		}

		std::construct_at(static_cast<Field*>(out.Data), StaticFieldValue<MetaField>::Value);
		return {};
	}

	template <std::meta::info MetaField>
	std::expected<void, FieldError> StaticFieldGetThunk(const TypedRef& out)
	{
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if (out.Type != &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaField))>())
		{
			return std::unexpected(FieldError{FieldError::TypeMismatch});
		}

		std::construct_at(static_cast<Field*>(out.Data), [:MetaField:]);
		return {};
	}

	template <std::meta::info MetaField>
	std::expected<void, FieldError> StaticFieldSetThunk(const TypedRef& in)
	{
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if (in.Type != &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaField))>())
		{
			return std::unexpected(FieldError{FieldError::TypeMismatch});
		}

		Field* source = static_cast<Field*>(in.Data);

		// Same rule as a non-static field: a move-only member is writable only through a move offered by a
		// mutable caller, a copy-only one only through a copy.
		const bool moveAllowed = in.Movable && !in.IsConst;
		if constexpr (std::is_copy_assignable_v<Field> && std::is_move_assignable_v<Field>)
		{
			if (moveAllowed)
			{
				[:MetaField:] = std::move(*source);
			}
			else
			{
				[:MetaField:] = *source;
			}
		}
		else if constexpr (std::is_move_assignable_v<Field>)
		{
			if (!moveAllowed)
			{
				return std::unexpected(FieldError{FieldError::NotWritable});
			}
			[:MetaField:] = std::move(*source);
		}
		else
		{
			[:MetaField:] = *source;
		}

		return {};
	}

	template <std::meta::info MetaField>
	TypedRef StaticFieldRefThunk()
	{
		auto&& lvalue = [:MetaField:];

		return TypedRef{.Type = &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaField))>(),
						.Data = const_cast<void*>(static_cast<const void*>(std::addressof(lvalue))),
						.IsConst = std::is_const_v<std::remove_reference_t<decltype(lvalue)>>};
	}

	template <std::meta::info MetaField>
	consteval StaticFieldGetter MakeStaticFieldGetter()
	{
		// A move-only member cannot be copied into the caller slot; it has no value getter (NotReadable).
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if constexpr (!std::is_copy_constructible_v<Field>)
		{
			return nullptr;
		}
		else if constexpr (ConstantReadable<MetaField>)
		{
			return &StaticFieldValueGetThunk<MetaField>;
		}
		else
		{
			return &StaticFieldGetThunk<MetaField>;
		}
	}

	template <std::meta::info MetaField>
	consteval StaticFieldSetter MakeStaticFieldSetter()
	{
		// A constant-readable member is a value, never a settable object: it may have no definition to
		// assign to, and it is const by construction. Everything else follows the non-static field rules.
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if constexpr (ConstantReadable<MetaField>)
		{
			return nullptr;
		}
		else if constexpr (!std::is_const_v<std::remove_reference_t<Declared>> && !std::is_reference_v<Declared> &&
						   (std::is_copy_assignable_v<Field> || std::is_move_assignable_v<Field>))
		{
			return &StaticFieldSetThunk<MetaField>;
		}
		else
		{
			return nullptr;
		}
	}

	template <std::meta::info MetaField>
	consteval StaticFieldReferencer MakeStaticFieldReferencer()
	{
		// The address of a constant-readable member is never taken: forming it is well-formed during
		// substitution and fails only at link, so requires cannot guard it. The value path is the guard.
		if constexpr (ConstantReadable<MetaField>)
		{
			return nullptr;
		}
		else
		{
			return &StaticFieldRefThunk<MetaField>;
		}
	}

	template <std::meta::info MetaField>
	consteval StaticFieldInfo MakeStaticField()
	{
		return StaticFieldInfo(TypeReferenceTo<std::meta::remove_cvref(std::meta::type_of(MetaField))>(), IdentifierOf(MetaField),
							   DisplayStringOf(MetaField), ScopePathOf<MetaField>(),
							   StaticFieldTraits{.Access = AccessOf(MetaField), .IsConstantReadable = ConstantReadable<MetaField>},
							   MakeStaticFieldGetter<MetaField>(), MakeStaticFieldSetter<MetaField>(), MakeStaticFieldReferencer<MetaField>(),
							   MakeAnnotations<MetaField>());
	}

	export template <std::meta::info MetaVariable>
	constexpr const StaticFieldInfo& VariableOfMeta()
	{
		// A static data member here would not be pointer-identical to the one in TypeOf<T>().GetStaticFields(),
		// breaking the one-instance-per-entity property consumers compare on.
		static_assert(!std::meta::is_class_member(MetaVariable),
					  "VariableOfMeta reflects namespace-scope variables; reach a static member through TypeOf<T>().GetStaticFields()");

		// A namespace-scope variable is a static data member without a class: no offset, no instance, and the
		// same constant-readable/addressable split, so it reuses StaticFieldInfo whole.
		static constexpr StaticFieldInfo Variable = MakeStaticField<MetaVariable>();
		return Variable;
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval auto MakeStaticFieldArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members =
			std::define_static_array(std::meta::static_data_members_of(MetaType, std::meta::access_context::unchecked()));
		return std::array<StaticFieldInfo, sizeof...(I)>{MakeStaticField<members[I]>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeStaticFieldsFromType()
	{
		if constexpr (ProvidesSupersedingFacet<MetaType>())
		{
			return std::array<StaticFieldInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto count =
				std::define_static_array(std::meta::static_data_members_of(MetaType, std::meta::access_context::unchecked())).size();
			return MakeStaticFieldArray<MetaType>(std::make_index_sequence<count>{});
		}
		else
		{
			return std::array<StaticFieldInfo, 0>{};
		}
	}
}
