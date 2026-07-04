module;

#include <meta>

export module PlaygroundEngine.Reflection:Builder;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :TypeInfo;
import :FieldInfo;
import :FuncInfo;
import :TypedRef;
import :Annotation;

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

	template <std::meta::info Anno>
	struct AnnotationConstant
	{
		// Materializes one annotation value into static storage, keyed by its reflection so
		// each distinct annotation gets its own constant with a stable, program-lifetime
		// address. The split alias keeps the spliced type out of a template argument
		// (GCC -Wtemplate-body).

		using Declared = [:std::meta::type_of(Anno):];
		using Type = std::remove_cvref_t<Declared>;
		static constexpr Type Value = std::meta::extract<Type>(Anno);
	};

	template <std::meta::info Anno>
	consteval AnnotationInfo MakeAnnotation()
	{
		// The type tag is &TypeOfMeta<...>(), the same instance a caller reaches through
		// &TypeOf<A>(), so a runtime GetAnnotation<A>() matches by pointer identity.
		return AnnotationInfo{
			.Type = &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(Anno))>(),
			.Value = &AnnotationConstant<Anno>::Value,
		};
	}

	consteval std::vector<std::meta::info> GetAnnotationList(const std::meta::info entity)
	{
		std::vector<std::meta::info> annotations;
		for (const std::meta::info annotation : std::meta::annotations_of(entity))
			annotations.push_back(annotation);
		return annotations;
	}

	template <std::meta::info Entity, std::size_t... I>
	consteval auto MakeNAnnotations(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto annotations = std::define_static_array(GetAnnotationList(Entity));
		return std::array<AnnotationInfo, sizeof...(I)>{MakeAnnotation<annotations[I]>()...};
	}

	template <std::meta::info Entity>
	constexpr std::span<const AnnotationInfo> MakeAnnotations()
	{
		constexpr auto count = GetAnnotationList(Entity).size();
		static constexpr auto Annotations = MakeNAnnotations<Entity>(std::make_index_sequence<count>{});
		return Annotations;
	}

	template <std::meta::info MetaTypeInfo, std::meta::info MetaMemberInfo>
	std::expected<void, FieldError> FieldGetThunk(const void* obj, const TypedRef out)
	{
		using Owner = [:MetaTypeInfo:];
		using Declared = [:std::meta::type_of(MetaMemberInfo):];
		using Field = std::remove_cvref_t<Declared>;

		if (out.Type != &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaMemberInfo))>())
			return std::unexpected(FieldError{FieldError::TypeMismatch});

		std::construct_at(static_cast<Field*>(out.Data), static_cast<const Owner*>(obj)->[:MetaMemberInfo:]);
		return {};
	}

	template <std::meta::info MetaTypeInfo, std::meta::info MetaMemberInfo>
	std::expected<void, FieldError> FieldSetThunk(void* obj, const TypedRef in)
	{
		using Owner = [:MetaTypeInfo:];
		using Declared = [:std::meta::type_of(MetaMemberInfo):];
		using Field = std::remove_cvref_t<Declared>;

		if (in.Type != &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaMemberInfo))>())
			return std::unexpected(FieldError{FieldError::TypeMismatch});

		Field* source = static_cast<Field*>(in.Data);

		// A move-only field is writable only through a move (in.Movable, and only from a mutable source);
		// a copy-only field only through a copy. A field that supports both picks by the caller's flag.
		const bool moveAllowed = in.Movable && !in.IsConst;
		if constexpr (std::is_copy_assignable_v<Field> && std::is_move_assignable_v<Field>)
		{
			if (moveAllowed)
				static_cast<Owner*>(obj)->[:MetaMemberInfo:] = std::move(*source);
			else
				static_cast<Owner*>(obj)->[:MetaMemberInfo:] = *source;
		}
		else if constexpr (std::is_move_assignable_v<Field>)
		{
			if (!moveAllowed)
				return std::unexpected(FieldError{FieldError::NotWritable});
			static_cast<Owner*>(obj)->[:MetaMemberInfo:] = std::move(*source);
		}
		else
			static_cast<Owner*>(obj)->[:MetaMemberInfo:] = *source;

		return {};
	}

	template <std::meta::info MetaTypeInfo, std::meta::info MetaMemberInfo>
	consteval FieldGetter MakeFieldGetter()
	{
		// A move-only member cannot be copied into the caller slot; it has no value getter (NotReadable).
		using Declared = [:std::meta::type_of(MetaMemberInfo):];
		using Field = std::remove_cvref_t<Declared>;
		if constexpr (std::is_copy_constructible_v<Field>)
			return &FieldGetThunk<MetaTypeInfo, MetaMemberInfo>;
		else
			return nullptr;
	}

	template <std::meta::info MetaTypeInfo, std::meta::info MetaMemberInfo>
	consteval FieldSetter MakeFieldSetter()
	{
		// const, reference, and non-assignable members have no value setter (NotWritable). A member that
		// is only move-assignable gets a setter that requires the caller's move flag.
		using Declared = [:std::meta::type_of(MetaMemberInfo):];
		using Field = std::remove_cvref_t<Declared>;
		if constexpr (!std::is_const_v<std::remove_reference_t<Declared>>
			&& !std::is_reference_v<Declared>
			&& (std::is_copy_assignable_v<Field> || std::is_move_assignable_v<Field>))
			return &FieldSetThunk<MetaTypeInfo, MetaMemberInfo>;
		else
			return nullptr;
	}

	template <std::meta::info MetaTypeInfo, std::meta::info MetaMemberInfo>
	TypedRef FieldRefThunk(void* obj)
	{
		using Owner = [:MetaTypeInfo:];

		// Bind the member access to a reference first: keeps the spliced member out of a template
		// argument and exposes the in-place const-ness via decltype (GCC -Wtemplate-body).
		auto&& lvalue = static_cast<Owner*>(obj)->[:MetaMemberInfo:];

		return TypedRef{
			.Type = &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaMemberInfo))>(),
			.Data = const_cast<void*>(static_cast<const void*>(std::addressof(lvalue))),
			.IsConst = std::is_const_v<std::remove_reference_t<decltype(lvalue)>>
		};
	}

	template <std::meta::info MetaTypeInfo, std::meta::info MetaMemberInfo>
	consteval FieldReferencer MakeFieldReferencer()
	{
		// A bitfield has no address, so it has no borrow (NotAddressable).
		if constexpr (std::meta::is_bit_field(MetaMemberInfo))
			return nullptr;
		else
			return &FieldRefThunk<MetaTypeInfo, MetaMemberInfo>;
	}

	template <std::meta::info MetaTypeInfo, const std::meta::info MetaMemberInfo>
	consteval FieldInfo MakeField()
	{
		const auto [bytes, bits] = std::meta::offset_of(MetaMemberInfo);
		return FieldInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaMemberInfo))>(),
		                 std::meta::identifier_of(MetaMemberInfo), bytes, bits,
		                 MakeFieldGetter<MetaTypeInfo, MetaMemberInfo>(),
		                 MakeFieldSetter<MetaTypeInfo, MetaMemberInfo>(),
		                 MakeFieldReferencer<MetaTypeInfo, MetaMemberInfo>(),
		                 MakeAnnotations<MetaMemberInfo>());
	}

	template <std::meta::info MetaTypeInfo, std::size_t... I>
	consteval auto MakeNFieldsFromType(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(MetaTypeInfo, std::meta::access_context::unchecked()));
		return std::array<FieldInfo, sizeof...(I)>{MakeField<MetaTypeInfo, members[I]>()...};
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
		return ParamInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaParamInfo))>(), name,
		                 MakeAnnotations<MetaParamInfo>());
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
		static constexpr auto Params = MakeNParams<MetaFuncInfo>(std::make_index_sequence<numParams>{});
		return Params;
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
			{
				// Copyable by-value parameter: copy by default, move only if the caller opted in, the
				// source is mutable, and the type actually has a usable move constructor. Both branches
				// yield a prvalue so decltype(auto) stays consistent.
				if constexpr (std::is_move_constructible_v<Value>)
				{
					if (arg.Movable && !arg.IsConst)
						return Value(std::move(*pointer));
				}
				return Value(*pointer);
			}
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

	template <typename T, std::meta::info Fn, std::size_t... I>
	decltype(auto) DoCall([[maybe_unused]] void* obj, [[maybe_unused]] std::span<const TypedRef> args,
	                      std::index_sequence<I...>)
	{
		// Bind arguments inline at the call so a by-value parameter is initialized directly from the
		// bound prvalue (guaranteed copy elision): no extra move, and a type with a deleted move
		// constructor still binds by copy.
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(Fn));
		if constexpr (std::meta::is_static_member(Fn))
			return [:Fn:](ArgBind<parameters[I]>::Bind(args[I])...);
		else
			return static_cast<T*>(obj)->[:Fn:](ArgBind<parameters[I]>::Bind(args[I])...);
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

			DoCall<T, Fn>(obj, args, std::index_sequence<I...>{});
		}
		else if constexpr (std::is_reference_v<Return>)
		{
			// A reference return is erased as a pointer to the referent, tagged as that pointer type
			// so it stays distinct from a same-typed value return.
			using Referent = std::remove_reference_t<Return>;
			if (ret.Type != nullptr && ret.Type != PointerReturnTag<Referent*>())
				return std::unexpected(InvokeError{InvokeError::ReturnTypeMismatch, 0});

			Return result = DoCall<T, Fn>(obj, args, std::index_sequence<I...>{});
			if (ret.Type != nullptr)
				*static_cast<Referent**>(ret.Data) = std::addressof(result);
		}
		else
		{
			const TypeInfo* returnTag =
				&TypeOfMeta<std::meta::remove_cvref(std::meta::return_type_of(Fn))>();
			if (ret.Type != nullptr && ret.Type != returnTag)
				return std::unexpected(InvokeError{InvokeError::ReturnTypeMismatch, 0});

			Return result = DoCall<T, Fn>(obj, args, std::index_sequence<I...>{});
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
		                &InvokeThunk<T, MetaFuncInfo>,
		                MakeAnnotations<MetaFuncInfo>());
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

		static constexpr auto Fields = GetFieldsFromType<MetaTypeInfo>();
		static constexpr auto Functions = GetFunctionsFromType<MetaTypeInfo>();

		static constexpr auto Annotations = MakeAnnotations<MetaTypeInfo>();

		// std::is_object_v excludes void (reached here as a function return type), where
		// StringifyValue can't form a const T*; it stays true for primitives and classes.
		if constexpr (Fields.empty() && std::is_object_v<T>)
		{
			static constexpr TypeInfo TypeInfo(displayName, Fields, Functions, &StringifyValue<T>, Annotations);
			return TypeInfo;
		}
		else
		{
			static constexpr TypeInfo TypeInfo(displayName, Fields, Functions, nullptr, Annotations);
			return TypeInfo;
		}
	}
}
