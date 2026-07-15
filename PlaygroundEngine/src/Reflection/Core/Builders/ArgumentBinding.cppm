module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:ArgumentBinding;

import :MetaCommon;
import :AnnotationsBuilder;
import :TypeInfo;
import :ParameterInfo;
import :TypedRef;
import :DeclarationInfo;

import std;

// The shared argument machinery for every erased callable (functions and constructors): parameter
// metadata, the type-driven per-argument binding (const/move/reference rules), and the neutral
// validation each caller maps to its own error type. See docs/ReflectionInternals.md (the argument binder).

namespace PgE::detail
{
	template <std::meta::info MetaParameter>
	struct ArgumentBinding;

	template <const std::meta::info MetaParameter>
	consteval ParameterTraits MakeParameterTraits()
	{
		// Read the qualifiers off the undecayed type: the language's own spelling, which the stored decayed
		// TypeReference loses. is_const is asked of the referenced type so const T& reports const, not the
		// reference's own (never present) constness.
		constexpr std::meta::info declared = std::meta::type_of(MetaParameter);

		return ParameterTraits{
			.IsConst = std::meta::is_const_type(std::meta::remove_reference(declared)),
			.IsLvalueReference = std::meta::is_lvalue_reference_type(declared),
			.IsRvalueReference = std::meta::is_rvalue_reference_type(declared),
			.HasDefaultArgument = std::meta::has_default_argument(MetaParameter),
			.BindsByMove = ArgumentBinding<MetaParameter>::NeedsMovable,
			.RequiresMutable = ArgumentBinding<MetaParameter>::NeedsMutable,
		};
	}

	template <const std::meta::info MetaParameter>
	consteval ParameterInfo MakeParameter()
	{
		return ParameterInfo(TypeReferenceTo<std::meta::remove_cvref(std::meta::type_of(MetaParameter))>(), IdentifierOf(MetaParameter),
							 DisplayStringOf(MetaParameter), ScopePathOf<MetaParameter>(), MakeParameterTraits<MetaParameter>(),
							 MakeAnnotations<MetaParameter>());
	}

	template <std::meta::info MetaCallable, std::size_t... I>
	consteval auto MakeParameterArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(MetaCallable));
		return std::array<ParameterInfo, sizeof...(I)>{MakeParameter<parameters[I]>()...};
	}

	template <std::meta::info MetaCallable>
	constexpr std::span<const ParameterInfo> MakeParameters()
	{
		constexpr auto parameterCount = std::meta::parameters_of(MetaCallable).size();
		static constexpr auto Parameters = MakeParameterArray<MetaCallable>(std::make_index_sequence<parameterCount>{});
		return Parameters;
	}

	template <std::meta::info MetaParameter>
	struct ArgumentBinding
	{
		// The single source of truth for how one parameter takes an erased argument. ParameterInfo
		// publishes NeedsMovable/NeedsMutable so callers can rank overloads without re-deriving them.
		using Parameter = [:std::meta::type_of(MetaParameter):];
		using Value = std::remove_reference_t<Parameter>;

		// The parameters Bind moves out of unconditionally, so the caller must have offered the argument.
		// A copyable by-value parameter is absent: it falls back to a copy rather than failing the call.
		static constexpr bool NeedsMovable =
			std::is_rvalue_reference_v<Parameter> ||
			(!std::is_reference_v<Parameter> && std::is_move_constructible_v<Value> && !std::is_copy_constructible_v<Value>);

		static constexpr bool NeedsMutable = (std::is_lvalue_reference_v<Parameter> && !std::is_const_v<Value>) || NeedsMovable;

		static const TypeInfo* ExpectedTag()
		{
			return &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaParameter))>();
		}

		static decltype(auto) Bind(const TypedRef& arg)
		{
			// The parameter's own type drives the call: references bind to the caller's object,
			// rvalue-ref and move-only value parameters move out of it, copyable value parameters copy.
			Value* pointer = static_cast<Value*>(arg.Data);

			if constexpr (std::is_rvalue_reference_v<Parameter>)
			{
				return std::move(*pointer);
			}
			else if constexpr (std::is_lvalue_reference_v<Parameter>)
			{
				return *pointer;
			}
			else if constexpr (std::is_move_constructible_v<Value> && !std::is_copy_constructible_v<Value>)
			{
				return std::move(*pointer);
			}
			else
			{
				// Copyable by-value parameter: copy by default, move only if the caller opted in, the
				// source is mutable, and the type actually has a usable move constructor. Both branches
				// yield a prvalue so decltype(auto) stays consistent.
				if constexpr (std::is_move_constructible_v<Value>)
				{
					if (arg.Movable && !arg.IsConst)
					{
						return Value(std::move(*pointer));
					}
				}
				return Value(*pointer);
			}
		}
	};

	export enum class ArgumentError : std::uint8_t
	{
		None,
		TypeMismatch,
		NullArgument,
		ConstViolation,
		NotMovable,
	};

	template <std::meta::info MetaParameter>
	ArgumentError CheckArgument(const TypedRef& arg)
	{
		// Neutral per-argument validation shared by every callable; the caller maps the result to its own
		// error enum. A free function (not a lambda) keeps the spliced parameter reflection in a
		// constant-evaluated context (GCC -Wtemplate-body).
		if (arg.Type != ArgumentBinding<MetaParameter>::ExpectedTag())
		{
			return ArgumentError::TypeMismatch;
		}

		// Every binding dereferences the argument, and Data is the address of the object itself (a null
		// pointer argument still has an address), so nothing reaches Bind meaning "no object".
		if (arg.Data == nullptr)
		{
			return ArgumentError::NullArgument;
		}

		if (ArgumentBinding<MetaParameter>::NeedsMutable && arg.IsConst)
		{
			return ArgumentError::ConstViolation;
		}

		if (ArgumentBinding<MetaParameter>::NeedsMovable && !arg.Movable)
		{
			return ArgumentError::NotMovable;
		}

		return ArgumentError::None;
	}
}
