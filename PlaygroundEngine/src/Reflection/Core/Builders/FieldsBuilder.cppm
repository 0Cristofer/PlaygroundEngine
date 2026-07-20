module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:FieldsBuilder;

import :MetaCommon;
import :FacetsBuilder;
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
	std::expected<void, FieldError> FieldGetThunk(const void* obj, const TypedRef& out)
	{
		using Owner = [:MetaType:];
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if (out.Type != &TypeMetaOf<std::meta::remove_cvref(std::meta::type_of(MetaField))>())
		{
			return std::unexpected(FieldError{FieldError::TypeMismatch});
		}

		std::construct_at(static_cast<Field*>(out.Data), static_cast<const Owner*>(obj)->[:MetaField:]);
		return {};
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	std::expected<void, FieldError> FieldSetThunk(void* obj, const TypedRef& in)
	{
		using Owner = [:MetaType:];
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if (in.Type != &TypeMetaOf<std::meta::remove_cvref(std::meta::type_of(MetaField))>())
		{
			return std::unexpected(FieldError{FieldError::TypeMismatch});
		}

		Field* source = static_cast<Field*>(in.Data);

		// Assignability is asked of the member itself, not of the decayed type: a volatile class-typed member is
		// not assignable from a plain value even though its decayed type is. MakeFieldSetter guards on exactly
		// these two questions, so the emitted body and the guard cannot disagree.
		constexpr bool copyAssignable = requires(Owner* owner, Field* value) { owner->[:MetaField:] = *value; };
		constexpr bool moveAssignable = requires(Owner* owner, Field* value) { owner->[:MetaField:] = std::move(*value); };

		// A move-only field is writable only through a move (in.Movable, and only from a mutable source);
		// a copy-only field only through a copy. A field that supports both picks by the caller's flag.
		const bool moveAllowed = in.Movable && !in.IsConst;
		if constexpr (copyAssignable && moveAssignable)
		{
			if (moveAllowed)
			{
				static_cast<Owner*>(obj)->[:MetaField:] = std::move(*source);
			}
			else
			{
				static_cast<Owner*>(obj)->[:MetaField:] = *source;
			}
		}
		else if constexpr (moveAssignable)
		{
			if (!moveAllowed)
			{
				return std::unexpected(FieldError{FieldError::NotWritable});
			}
			static_cast<Owner*>(obj)->[:MetaField:] = std::move(*source);
		}
		else
		{
			static_cast<Owner*>(obj)->[:MetaField:] = *source;
		}

		return {};
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	consteval FieldGetter MakeFieldGetter()
	{
		// Mirror FieldGetThunk exactly rather than approximating it with a trait on the decayed type: a
		// move-only member cannot be copied into the caller slot, and neither can a volatile class-typed one
		// (no copy constructor binds a volatile lvalue). Both reflect with no value getter (NotReadable).
		using Owner = [:MetaType:];
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		if constexpr (requires(const Owner* owner, Field* slot) { std::construct_at(slot, owner->[:MetaField:]); })
		{
			return &FieldGetThunk<MetaType, MetaField>;
		}
		else
		{
			return nullptr;
		}
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	consteval FieldSetter MakeFieldSetter()
	{
		// const, reference, and non-assignable members have no value setter (NotWritable). A member that
		// is only move-assignable gets a setter that requires the caller's move flag.
		using Owner = [:MetaType:];
		using Declared = [:std::meta::type_of(MetaField):];
		using Field = std::remove_cvref_t<Declared>;

		// FieldSetThunk's own two questions, asked of the member rather than of the decayed type: a volatile
		// class-typed member is assignable in its decayed form but not through the member, so guarding on the
		// decayed type emitted a body that could not compile.
		constexpr bool copyAssignable = requires(Owner* owner, Field* value) { owner->[:MetaField:] = *value; };
		constexpr bool moveAssignable = requires(Owner* owner, Field* value) { owner->[:MetaField:] = std::move(*value); };

		// A reference member is excluded by decision, not by assignability: assigning through it would write the
		// referent, and a reference member is never rebindable.
		if constexpr (std::is_const_v<std::remove_reference_t<Declared>> || std::is_reference_v<Declared>)
		{
			return nullptr;
		}
		else if constexpr (copyAssignable || moveAssignable)
		{
			return &FieldSetThunk<MetaType, MetaField>;
		}
		else
		{
			return nullptr;
		}
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	TypedRef FieldRefThunk(void* obj)
	{
		// Offset arithmetic, never a member splice, so the borrow is total: naming a deprecated member would
		// fire -Wdeprecated-declarations at build time, and it odr-uses no copy constructor a value get would.
		// See docs/ReflectionInternals.md (field access).
		constexpr std::meta::info declared = std::meta::type_of(MetaField);
		constexpr std::size_t byteOffset = std::meta::offset_of(MetaField).bytes;

		std::byte* storage = static_cast<std::byte*>(obj) + byteOffset;

		void* data;
		if constexpr (std::meta::is_reference_type(declared))
		{
			// A reference member stores a pointer to its referent; the borrow is the referent's address, the
			// same lvalue the old member splice yielded, so a reference field reads through one indirection.
			data = *reinterpret_cast<void**>(storage);
		}
		else
		{
			data = storage;
		}

		return TypedRef{.Type = &TypeMetaOf<std::meta::remove_cvref(declared)>(),
						.Data = data,
						.IsConst = std::meta::is_const_type(std::meta::remove_reference(declared))};
	}

	template <std::meta::info MetaType, std::meta::info MetaField>
	consteval FieldReferencer MakeFieldReferencer()
	{
		// A bitfield has no address, so it has no borrow (NotAddressable).
		if constexpr (std::meta::is_bit_field(MetaField))
		{
			return nullptr;
		}
		else
		{
			return &FieldRefThunk<MetaType, MetaField>;
		}
	}

	template <const std::meta::info MetaField>
	consteval FieldTraits MakeFieldTraits()
	{
		const auto [bytes, bits] = std::meta::offset_of(MetaField);
		const bool isBitField = std::meta::is_bit_field(MetaField);

		// Read the qualifiers off the undecayed declared type, as MakeParameterTraits does: is_const is asked
		// of the referenced type so const int& reports const, not the reference's own (never present) constness.
		constexpr std::meta::info declared = std::meta::type_of(MetaField);

		return FieldTraits{
			.Access = AccessOf(MetaField),
			.ByteOffset = static_cast<int>(bytes),
			.BitOffset = static_cast<int>(bits),
			.IsBitField = isBitField,
			// bit_size_of is only defined for a bitfield, so the query is guarded, not merely filtered.
			.BitSize = isBitField ? static_cast<int>(std::meta::bit_size_of(MetaField)) : 0,
			.HasDefaultInitializer = std::meta::has_default_member_initializer(MetaField),
			.IsMutable = std::meta::is_mutable_member(MetaField),
			.IsConst = std::meta::is_const_type(std::meta::remove_reference(declared)),
			.IsVolatile = std::meta::is_volatile_type(std::meta::remove_reference(declared)),
			.IsLvalueReference = std::meta::is_lvalue_reference_type(declared),
			.IsRvalueReference = std::meta::is_rvalue_reference_type(declared),
		};
	}

	template <std::meta::info MetaType, const std::meta::info MetaField>
	consteval FieldInfo MakeField()
	{
		return FieldInfo(TypeReferenceTo<std::meta::remove_cvref(std::meta::type_of(MetaField))>(), IdentifierOf(MetaField),
						 DisplayStringOf(MetaField), ScopePathOf<MetaField>(), MakeFieldTraits<MetaField>(), MakeFieldGetter<MetaType, MetaField>(),
						 MakeFieldSetter<MetaType, MetaField>(), MakeFieldReferencer<MetaType, MetaField>(), MakeAnnotations<MetaField>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval std::array<FieldInfo, sizeof...(I)> MakeFieldArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members =
			std::define_static_array(std::meta::nonstatic_data_members_of(MetaType, std::meta::access_context::unchecked()));
		return std::array<FieldInfo, sizeof...(I)>{MakeField<MetaType, members[I]>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeFieldsFromType()
	{
		if constexpr (ProvidesSupersedingFacet<MetaType>())
		{
			return std::array<FieldInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto fieldCount =
				std::define_static_array(std::meta::nonstatic_data_members_of(MetaType, std::meta::access_context::unchecked())).size();
			return MakeFieldArray<MetaType>(std::make_index_sequence<fieldCount>{});
		}
		else
		{
			return std::array<FieldInfo, 0>{};
		}
	}
}
