module;

#include <meta>

export module PlaygroundEngine.Reflection:Builder;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :TypeInfo;
import :FieldInfo;
import :FuncInfo;

import std;

// This file contains the compile-time reflection mechanisms to build the runtime reflection types

namespace PgE::detail
{
	template <std::meta::info MetaTypeInfo>
	constexpr const TypeInfo& TypeOfMeta();

	template <typename T>
	std::string StringifyValue(const void* obj)
	{
		return TypeInfoTraits<T>::Stringify(*static_cast<const T*>(obj));
	}

	template <const std::meta::info MetaMemberInfo>
	consteval FieldInfo MakeField()
	{
		const auto [bytes, bits] = std::meta::offset_of(MetaMemberInfo);
		return FieldInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaMemberInfo))>(),
		                 std::meta::display_string_of(MetaMemberInfo), bytes, bits);
	}

	template <std::meta::info MetaTypeInfo, std::size_t... I>
	consteval auto MakeNFieldsFromType(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(MetaTypeInfo, std::meta::access_context::unchecked()));
		return std::array<FieldInfo, sizeof...(I)>{MakeField<members[I]>()...};
	}

	template <std::meta::info MetaTypeInfo>
	consteval auto GetFieldsFromType()
	{
		using T = [:MetaTypeInfo:];
		if constexpr (TypeInfoTraits<T>::IS_LEAF)
		{
			return std::array<FieldInfo, 0>{};
		}
		else if constexpr (std::meta::is_class_type(MetaTypeInfo))
		{
			constexpr auto numMembers = std::define_static_array(
				std::meta::nonstatic_data_members_of(MetaTypeInfo, std::meta::access_context::unchecked())).size();
			return MakeNFieldsFromType<MetaTypeInfo>(std::make_index_sequence<numMembers>{});
		}
		else
		{
			return std::array<FieldInfo, 0>{};
		}
	}

	consteval std::vector<std::meta::info> GetMemberFunctions(const std::meta::info type)
	{
		// Excludes constructors, destructors, operators, and other special members.
		std::vector<std::meta::info> functions;
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			if (std::meta::is_function(member)
				&& !std::meta::is_special_member_function(member)
				&& !std::meta::is_constructor(member)
				&& !std::meta::is_destructor(member)
				&& !std::meta::is_operator_function(member))
			{
				functions.push_back(member);
			}
		}
		return functions;
	}

	template <const std::meta::info MetaParamInfo>
	consteval ParamInfo MakeParam()
	{
		constexpr std::string_view name = std::meta::has_identifier(MetaParamInfo)
			                                  ? std::meta::identifier_of(MetaParamInfo)
			                                  : std::string_view{};
		return ParamInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaParamInfo))>(), name);
	}

	template <std::meta::info MetaFuncInfo, std::size_t... I>
	consteval auto MakeNParams(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto params = std::define_static_array(std::meta::parameters_of(MetaFuncInfo));
		return std::array<ParamInfo, sizeof...(I)>{MakeParam<params[I]>()...};
	}

	template <std::meta::info MetaFuncInfo>
	constexpr std::span<const ParamInfo> MakeParams()
	{
		constexpr auto numParams = std::meta::parameters_of(MetaFuncInfo).size();
		static constexpr auto params = MakeNParams<MetaFuncInfo>(std::make_index_sequence<numParams>{});
		return params;
	}

	template <std::meta::info Param>
	struct ArgBind
	{
		using Parameter = [:std::meta::type_of(Param):];
		using Value = std::remove_reference_t<Parameter>;

		static constexpr bool NEEDS_MUTABLE =
			(std::is_lvalue_reference_v<Parameter> && !std::is_const_v<Value>)
			|| std::is_rvalue_reference_v<Parameter>
			|| (!std::is_reference_v<Parameter> && std::is_move_constructible_v<Value>
				&& !std::is_copy_constructible_v<Value>);

		static const TypeInfo* ExpectedTag()
		{
			return &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(Param))>();
		}

		static decltype(auto) Bind(const TypedRef& arg)
		{
			// The parameter's own type drives the call: references bind to the caller's object,
			// rvalue-ref and move-only value parameters move out of it, copyable value parameters copy.
			Value* pointer = static_cast<Value*>(arg.Data);

			if constexpr (std::is_rvalue_reference_v<Parameter>)
				return std::move(*pointer);
			else if constexpr (std::is_lvalue_reference_v<Parameter>)
				return *pointer;
			else if constexpr (std::is_move_constructible_v<Value> && !std::is_copy_constructible_v<Value>)
				return std::move(*pointer);
			else
				return *pointer;
		}
	};

	template <std::meta::info Param>
	void CheckArg(const TypedRef& arg, const std::size_t index, bool& valid, InvokeError& error)
	{
		if (!valid)
			return;

		if (arg.Type != ArgBind<Param>::ExpectedTag())
		{
			valid = false;
			error = {.Reason = InvokeError::TypeMismatch, .ArgumentIndex = static_cast<std::uint16_t>(index)};
			return;
		}

		if (ArgBind<Param>::NEEDS_MUTABLE && arg.IsConst)
		{
			valid = false;
			error = {.Reason = InvokeError::ConstViolation, .ArgumentIndex = static_cast<std::uint16_t>(index)};
		}
	}

	template <typename T, std::meta::info Fn, typename... Arguments>
	decltype(auto) DoCall([[maybe_unused]] void* obj, Arguments&&... arguments)
	{
		if constexpr (std::meta::is_static_member(Fn))
			return [:Fn:](std::forward<Arguments>(arguments)...);
		else
			return static_cast<T*>(obj)->[:Fn:](std::forward<Arguments>(arguments)...);
	}

	template <typename PointerType>
	const TypeInfo* PointerReturnTag()
	{
		return &TypeOfMeta<^^PointerType>();
	}

	template <typename T, std::meta::info Fn, std::size_t... I>
	std::expected<void, InvokeError> InvokeThunkImpl([[maybe_unused]] void* obj,
	                                                 [[maybe_unused]] std::span<const TypedRef> args,
	                                                 [[maybe_unused]] const TypedRef ret,
	                                                 std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto parameters =
			std::define_static_array(std::meta::parameters_of(Fn));

		bool valid = true;
		InvokeError error{};

		// CheckArg is a free function, not a lambda, so the spliced parameter reflections stay in a
		// constant-evaluated context (GCC -Wtemplate-body).
		(CheckArg<parameters[I]>(args[I], I, valid, error), ...);
		if (!valid)
			return std::unexpected(error);

		using Return = [:std::meta::return_type_of(Fn):];
		if constexpr (std::is_void_v<Return>)
		{
			if (ret.Type != nullptr)
				return std::unexpected(InvokeError{.Reason = InvokeError::ReturnTypeMismatch, .ArgumentIndex = 0});

			DoCall<T, Fn>(obj, ArgBind<parameters[I]>::Bind(args[I])...);
		}
		else if constexpr (std::is_reference_v<Return>)
		{
			// A reference return is erased as a pointer to the referent, tagged as that pointer type
			// so it stays distinct from a same-typed value return.
			using Referent = std::remove_reference_t<Return>;
			if (ret.Type != nullptr && ret.Type != PointerReturnTag<Referent*>())
				return std::unexpected(InvokeError{InvokeError::ReturnTypeMismatch, 0});

			Return result = DoCall<T, Fn>(obj, ArgBind<parameters[I]>::Bind(args[I])...);
			if (ret.Type != nullptr)
				*static_cast<Referent**>(ret.Data) = std::addressof(result);
		}
		else
		{
			const TypeInfo* returnTag =
				&TypeOfMeta<std::meta::remove_cvref(std::meta::return_type_of(Fn))>();
			if (ret.Type != nullptr && ret.Type != returnTag)
				return std::unexpected(InvokeError{InvokeError::ReturnTypeMismatch, 0});

			Return result = DoCall<T, Fn>(obj, ArgBind<parameters[I]>::Bind(args[I])...);
			if (ret.Type != nullptr)
				std::construct_at(static_cast<Return*>(ret.Data), std::move(result));
		}

		return {};
	}

	template <typename T, std::meta::info Fn>
	std::expected<void, InvokeError> InvokeThunk(void* obj, std::span<const TypedRef> args, TypedRef ret)
	{
		constexpr std::size_t parameterCount = std::meta::parameters_of(Fn).size();
		if (args.size() != parameterCount)
			return std::unexpected(InvokeError{InvokeError::ArityMismatch, 0});

		return InvokeThunkImpl<T, Fn>(obj, args, ret, std::make_index_sequence<parameterCount>{});
	}

	template <std::meta::info MetaTypeInfo, std::meta::info MetaFuncInfo>
	consteval FuncInfo MakeFunction()
	{
		using T = [:MetaTypeInfo:];
		const bool constCallable =
			std::meta::is_const(MetaFuncInfo) || std::meta::is_static_member(MetaFuncInfo);

		return FuncInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::return_type_of(MetaFuncInfo))>(),
		                std::meta::identifier_of(MetaFuncInfo),
		                MakeParams<MetaFuncInfo>(),
		                constCallable,
		                &InvokeThunk<T, MetaFuncInfo>);
	}

	template <std::meta::info MetaTypeInfo, std::size_t... I>
	consteval auto MakeNFunctionsFromType(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto functions = std::define_static_array(GetMemberFunctions(MetaTypeInfo));
		return std::array<FuncInfo, sizeof...(I)>{MakeFunction<MetaTypeInfo, functions[I]>()...};
	}

	template <std::meta::info MetaTypeInfo>
	consteval auto GetFunctionsFromType()
	{
		using T = [:MetaTypeInfo:];
		if constexpr (TypeInfoTraits<T>::IS_LEAF)
		{
			return std::array<FuncInfo, 0>{};
		}
		else if constexpr (std::meta::is_class_type(MetaTypeInfo))
		{
			constexpr auto numFunctions = std::define_static_array(GetMemberFunctions(MetaTypeInfo)).size();
			return MakeNFunctionsFromType<MetaTypeInfo>(std::make_index_sequence<numFunctions>{});
		}
		else
		{
			return std::array<FuncInfo, 0>{};
		}
	}

	template <std::meta::info MetaTypeInfo>
	constexpr const TypeInfo& TypeOfMeta()
	{
		using T = [:MetaTypeInfo:];
		constexpr std::string_view displayName = std::meta::display_string_of(MetaTypeInfo);

		// ReSharper disable CppInconsistentNaming
		static constexpr auto fields = GetFieldsFromType<MetaTypeInfo>();
		static constexpr auto functions = GetFunctionsFromType<MetaTypeInfo>();

		// std::is_object_v excludes void (reached here as a function return type), where
		// StringifyValue can't form a const T*; it stays true for primitives and classes.
		if constexpr (fields.empty() && std::is_object_v<T>)
		{
			static constexpr TypeInfo typeInfo(displayName, fields, functions, &StringifyValue<T>);
			return typeInfo;
		}
		else
		{
			static constexpr TypeInfo typeInfo(displayName, fields, functions, nullptr);
			return typeInfo;
		}
		// ReSharper restore CppInconsistentNaming
	}
}
