module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:FunctionSignatureBuilder;

import :MetaCommon;
import :FunctionSignatureInfo;

import std;

// Builds the FunctionSignatureInfo for a function type, so a walk that reaches void(int, float) through a
// pointer, a parameter, or a template argument can still say what it takes and returns. Without it the
// function node is a dead end. See docs/ReflectionExtraction.md (function types).

namespace PgE::detail
{
	// Only the ellipsis forms are listed; every non-variadic function type falls through to the primary. Cv and
	// ref qualifiers are independent of the ellipsis, so each combination needs its own specialization, and
	// std::meta offers no is_variadic to ask instead. See docs/ReflectionExtraction.md (function types).
	template <typename Function>
	struct FunctionTypeIsVariadic : std::false_type
	{};

#define PGE_REFLECT_VARIADIC_FUNCTION_TYPE(Qualifiers)                                                                                               \
	template <typename Return, typename... Parameters>                                                                                               \
	struct FunctionTypeIsVariadic<Return(Parameters..., ...) Qualifiers> : std::true_type                                                            \
	{};

	PGE_REFLECT_VARIADIC_FUNCTION_TYPE()
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(&)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(&&)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const&)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const&&)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(volatile)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(volatile&)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(volatile&&)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const volatile)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const volatile&)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const volatile&&)

	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(& noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(&& noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const& noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const&& noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(volatile noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(volatile& noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(volatile&& noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const volatile noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const volatile& noexcept)
	PGE_REFLECT_VARIADIC_FUNCTION_TYPE(const volatile&& noexcept)

#undef PGE_REFLECT_VARIADIC_FUNCTION_TYPE

	template <std::meta::info MetaType, std::size_t... I>
	consteval std::array<TypeReference, sizeof...(I)> MakeSignatureParameterArray(std::index_sequence<I...>)
	{
		// parameters_of on a function TYPE yields the parameter types themselves, unlike on a function
		// declaration where it yields parameter declarations that type_of must be applied to. Asking type_of
		// here throws, and a requires-expression does not catch that. See docs/ReflectionExtraction.md.
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(MetaType));
		return std::array<TypeReference, sizeof...(I)>{TypeReferenceTo<parameters[I]>()...};
	}

	template <std::meta::info MetaType>
	constexpr std::span<const TypeReference> MakeSignatureParameters()
	{
		constexpr auto count = std::define_static_array(std::meta::parameters_of(MetaType)).size();
		static constexpr auto Parameters = MakeSignatureParameterArray<MetaType>(std::make_index_sequence<count>{});
		return Parameters;
	}

	template <std::meta::info MetaType>
	constexpr const FunctionSignatureInfo& FunctionSignatureOfMeta()
	{
		using Function = [:MetaType:];

		static constexpr FunctionSignatureInfo Signature(TypeReferenceTo<std::meta::return_type_of(MetaType)>(), MakeSignatureParameters<MetaType>(),
														 std::meta::is_noexcept(MetaType), FunctionTypeIsVariadic<Function>::value);
		return Signature;
	}

	template <std::meta::info MetaType>
	consteval const FunctionSignatureInfo* MakeFunctionSignature()
	{
		if constexpr (std::meta::is_function_type(MetaType))
		{
			return &FunctionSignatureOfMeta<MetaType>();
		}
		else
		{
			return nullptr;
		}
	}
}
