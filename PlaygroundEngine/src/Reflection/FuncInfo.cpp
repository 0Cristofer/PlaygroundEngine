module PlaygroundEngine.Reflection;

import :FuncInfo;
import :TypedRef;

import std;

namespace PgE
{
	const TypeInfo& ParamInfo::GetTypeInfo() const
	{
		return *_typeInfo;
	}

	const TypeInfo& FuncInfo::GetReturnType() const
	{
		return *_returnType;
	}

	std::span<const ParamInfo> FuncInfo::GetParams() const
	{
		return _params;
	}

	bool FuncInfo::IsConst() const
	{
		return _constCallable;
	}

	// ReSharper disable CppPassValueParameterByConstReference

	std::expected<void, InvokeError> FuncInfo::Invoke(void* obj, const std::span<const TypedRef> args,
	                                                  const TypedRef ret) const
	{
		return _invoke(obj, args, ret);
	}

	std::expected<void, InvokeError> FuncInfo::Invoke(const void* obj, const std::span<const TypedRef> args,
	                                                  const TypedRef ret) const
	{
		if (!IsConst())
			return std::unexpected(InvokeError{InvokeError::ConstViolation, 0});

		return _invoke(const_cast<void*>(obj), args, ret);
	}

	// ReSharper restore CppPassValueParameterByConstReference
}
