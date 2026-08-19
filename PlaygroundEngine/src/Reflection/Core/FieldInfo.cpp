module PlaygroundEngine.Reflection.Core;

import :FieldInfo;
import :TypedRef;

import std;

namespace PgE
{
	int FieldInfo::GetByteOffset() const
	{
		return _traits.ByteOffset;
	}

	const TypeInfo& FieldInfo::GetTypeInfo() const
	{
		return _typeInfo.Get();
	}

	const TypeInfo& FieldInfo::GetDeclaringType() const
	{
		return _declaringType.Get();
	}

	std::expected<void, FieldError> FieldInfo::CheckDeclaringInstance(const TypedRef& object) const
	{
		if (object.Data == nullptr)
		{
			return std::unexpected(FieldError{FieldError::NullObject});
		}
		if (object.Type != &GetDeclaringType())
		{
			return std::unexpected(FieldError{FieldError::ObjectTypeMismatch});
		}

		return {};
	}

	std::expected<void, FieldError> FieldInfo::GetValue(const TypedRef& object, const TypedRef& out) const
	{
		if (const auto checked = CheckDeclaringInstance(object); !checked)
		{
			return checked;
		}
		if (!_getter)
		{
			return std::unexpected(FieldError{FieldError::NotReadable});
		}
		if (out.IsConst)
		{
			return std::unexpected(FieldError{FieldError::ConstViolation});
		}

		return _getter(object.Data, out);
	}

	std::expected<void, FieldError> FieldInfo::SetValue(const TypedRef& object, const TypedRef& in) const
	{
		if (const auto checked = CheckDeclaringInstance(object); !checked)
		{
			return checked;
		}
		if (object.IsConst && !WritableThroughConstObject())
		{
			return std::unexpected(FieldError{FieldError::ConstViolation});
		}
		if (!_setter)
		{
			return std::unexpected(FieldError{FieldError::NotWritable});
		}

		return _setter(object.Data, in);
	}

	std::expected<TypedRef, FieldError> FieldInfo::GetRef(const TypedRef& object) const
	{
		if (const auto checked = CheckDeclaringInstance(object); !checked)
		{
			return std::unexpected(checked.error());
		}
		if (!_referencer)
		{
			return std::unexpected(FieldError{FieldError::NotAddressable});
		}

		// The object's constness reaches the field, and stops where C++ stops it: at a mutable member, and
		// at a reference member whose referent the qualification never reached.
		TypedRef ref = _referencer(object.Data);
		ref.IsConst = ref.IsConst || (object.IsConst && !WritableThroughConstObject());
		return ref;
	}
}
