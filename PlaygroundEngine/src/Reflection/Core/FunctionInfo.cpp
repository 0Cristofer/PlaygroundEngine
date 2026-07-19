module PlaygroundEngine.Reflection.Core;

import :FunctionInfo;
import :TypedRef;

import std;

namespace PgE
{
	const TypeInfo& FunctionInfo::GetReturnType() const
	{
		return _returnType.Get();
	}

	std::span<const ParameterInfo> FunctionInfo::GetParams() const
	{
		return _params;
	}

	std::expected<void, InvokeError> FunctionInfo::Invoke(void* obj, const std::span<const TypedRef> args, const TypedRef& ret) const
	{
		if (!_invoke)
		{
			return std::unexpected(InvokeError{.Reason = InvokeError::NotInvocable, .ArgumentIndex = 0});
		}

		return _invoke(obj, args, ret);
	}

	std::expected<void, InvokeError> FunctionInfo::Invoke(const void* obj, const std::span<const TypedRef> args, const TypedRef& ret) const
	{
		if (!_invoke)
		{
			return std::unexpected(InvokeError{.Reason = InvokeError::NotInvocable, .ArgumentIndex = 0});
		}

		if (!IsConstCallable())
		{
			return std::unexpected(InvokeError{InvokeError::ConstViolation, 0});
		}

		return _invoke(const_cast<void*>(obj), args, ret);
	}
}
