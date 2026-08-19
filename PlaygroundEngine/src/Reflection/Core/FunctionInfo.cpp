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

	std::expected<void, InvokeError> FunctionInfo::Invoke(const TypedRef& object, const std::span<const TypedRef> args, const TypedRef& ret) const
	{
		if (!_invoke)
		{
			return std::unexpected(InvokeError{.Reason = InvokeError::NotInvocable, .ArgumentIndex = 0});
		}

		// A static or free function ignores the object entirely: the thunk never dereferences it, and the
		// caller may have nothing meaningful to hand over.
		if (!CallsWithoutObject())
		{
			if (object.Data == nullptr)
			{
				return std::unexpected(InvokeError{InvokeError::ObjectRequired, 0});
			}
			if (object.Type != GetDeclaringType())
			{
				return std::unexpected(InvokeError{InvokeError::ObjectTypeMismatch, 0});
			}
			if (object.IsConst && !IsConstCallable())
			{
				return std::unexpected(InvokeError{InvokeError::ConstViolation, 0});
			}
		}

		// The destination is a borrow too: a read-only slot cannot receive the return value.
		if (ret.IsConst)
		{
			return std::unexpected(InvokeError{InvokeError::ConstViolation, 0});
		}

		return _invoke(object.Data, args, ret);
	}

	std::expected<void, InvokeError> FunctionInfo::Invoke(const std::span<const TypedRef> args, const TypedRef& ret) const
	{
		if (!CallsWithoutObject())
		{
			return std::unexpected(InvokeError{InvokeError::ObjectRequired, 0});
		}

		return Invoke(TypedRef{}, args, ret);
	}
}
