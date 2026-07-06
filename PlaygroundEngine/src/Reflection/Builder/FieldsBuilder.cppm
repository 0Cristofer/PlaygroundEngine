module;

#include <meta>

export module PlaygroundEngine.Reflection:FieldsBuilder;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :MetaCommon;
import :AnnotationsBuilder;
import :TypeInfo;
import :FieldInfo;
import :TypedRef;
import :DeclarationInfo;

import std;

// Builds the FieldInfo list for a type: the runtime get/set/reference thunks and the compile-time
// selection of which of them a given member actually supports (const, reference, move-only, bitfield).

namespace PgE::detail
{
	template <std::meta::info MetaType, std::meta::info MetaField>
	std::expected<void, FieldError> FieldGetThunk(const void* obj, const TypedRef out)
	{
		using Owner = [:MetaType:];
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if (out.Type != &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaField))>())
			return std::unexpected(FieldError{FieldError::TypeMismatch});

		std::construct_at(static_cast<Field*>(out.Data), static_cast<const Owner*>(obj)->[:MetaField:]);
		return {};
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	std::expected<void, FieldError> FieldSetThunk(void* obj, const TypedRef in)
	{
		using Owner = [:MetaType:];
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if (in.Type != &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaField))>())
			return std::unexpected(FieldError{FieldError::TypeMismatch});

		Field* source = static_cast<Field*>(in.Data);

		// A move-only field is writable only through a move (in.Movable, and only from a mutable source);
		// a copy-only field only through a copy. A field that supports both picks by the caller's flag.
		const bool moveAllowed = in.Movable && !in.IsConst;
		if constexpr (std::is_copy_assignable_v<Field> && std::is_move_assignable_v<Field>)
		{
			if (moveAllowed)
				static_cast<Owner*>(obj)->[:MetaField:] = std::move(*source);
			else
				static_cast<Owner*>(obj)->[:MetaField:] = *source;
		}
		else if constexpr (std::is_move_assignable_v<Field>)
		{
			if (!moveAllowed)
				return std::unexpected(FieldError{FieldError::NotWritable});
			static_cast<Owner*>(obj)->[:MetaField:] = std::move(*source);
		}
		else
			static_cast<Owner*>(obj)->[:MetaField:] = *source;

		return {};
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	consteval FieldGetter MakeFieldGetter()
	{
		// A move-only member cannot be copied into the caller slot; it has no value getter (NotReadable).
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;
		if constexpr (std::is_copy_constructible_v<Field>)
			return &FieldGetThunk<MetaType, MetaField>;
		else
			return nullptr;
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	consteval FieldSetter MakeFieldSetter()
	{
		// const, reference, and non-assignable members have no value setter (NotWritable). A member that
		// is only move-assignable gets a setter that requires the caller's move flag.
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;
		if constexpr (!std::is_const_v<std::remove_reference_t<Declared>>
			&& !std::is_reference_v<Declared>
			&& (std::is_copy_assignable_v<Field> || std::is_move_assignable_v<Field>))
			return &FieldSetThunk<MetaType, MetaField>;
		else
			return nullptr;
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	TypedRef FieldRefThunk(void* obj)
	{
		using Owner = [:MetaType:];

		// Bind the member access to a reference first: keeps the spliced member out of a template
		// argument and exposes the in-place const-ness via decltype (GCC -Wtemplate-body).
		auto&& lvalue = static_cast<Owner*>(obj)->[:MetaField:];

		return TypedRef{
			.Type = &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaField))>(),
			.Data = const_cast<void*>(static_cast<const void*>(std::addressof(lvalue))),
			.IsConst = std::is_const_v<std::remove_reference_t<decltype(lvalue)>>
		};
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	consteval FieldReferencer MakeFieldReferencer()
	{
		// A bitfield has no address, so it has no borrow (NotAddressable).
		if constexpr (std::meta::is_bit_field(MetaField))
			return nullptr;
		else
			return &FieldRefThunk<MetaType, MetaField>;
	}

	template <std::meta::info MetaType, const std::meta::info MetaField>
	consteval FieldInfo MakeField()
	{
		const auto [bytes, bits] = std::meta::offset_of(MetaField);
		return FieldInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaField))>(),
		                 IdentifierOf(MetaField), DisplayStringOf(MetaField),
		                 bytes, bits,
		                 MakeFieldGetter<MetaType, MetaField>(),
		                 MakeFieldSetter<MetaType, MetaField>(),
		                 MakeFieldReferencer<MetaType, MetaField>(),
		                 MakeAnnotations<MetaField>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval auto MakeFieldArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(MetaType, std::meta::access_context::unchecked()));
		return std::array<FieldInfo, sizeof...(I)>{MakeField<MetaType, members[I]>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeFieldsFromType()
	{
		using T = [:MetaType:];
		if constexpr (TypeInfoTraits<T>::IsLeaf)
		{
			return std::array<FieldInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto fieldCount = std::define_static_array(
				std::meta::nonstatic_data_members_of(MetaType, std::meta::access_context::unchecked())).size();
			return MakeFieldArray<MetaType>(std::make_index_sequence<fieldCount>{});
		}
		else
		{
			return std::array<FieldInfo, 0>{};
		}
	}
}
