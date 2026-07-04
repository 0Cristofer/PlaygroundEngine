module PlaygroundEngine.Reflection;

import :FunctionInfo;
import :TypedRef;

import std;

namespace PgE
{
	const TypeInfo& ParameterInfo::GetTypeInfo() const
	{
		return *_typeInfo;
	}

	const TypeInfo& FunctionInfo::GetReturnType() const
	{
		return *_returnType;
	}

	std::span<const ParameterInfo> FunctionInfo::GetParams() const
	{
		return _params;
	}

	bool FunctionInfo::IsConst() const
	{
		return _constCallable;
	}

	// ReSharper disable CppPassValueParameterByConstReference

	std::expected<void, InvokeError> FunctionInfo::Invoke(void* obj, const std::span<const TypedRef> args,
	                                                  const TypedRef ret) const
	{
		return _invoke(obj, args, ret);
	}

	std::expected<void, InvokeError> FunctionInfo::Invoke(const void* obj, const std::span<const TypedRef> args,
	                                                  const TypedRef ret) const
	{
		if (!IsConst())
			return std::unexpected(InvokeError{InvokeError::ConstViolation, 0});

		return _invoke(const_cast<void*>(obj), args, ret);
	}

	// ReSharper restore CppPassValueParameterByConstReference
}
