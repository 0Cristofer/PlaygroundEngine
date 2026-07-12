module PlaygroundEngine.Reflection.Core;

import :FieldInfo;
import :TypedRef;

import std;

namespace PgE
{
	int FieldInfo::GetByteOffset() const
	{
		return _byteOffset;
	}

	const TypeInfo& FieldInfo::GetTypeInfo() const
	{
		return _typeInfo.Get();
	}

	std::expected<void, FieldError> FieldInfo::GetValue(const void* obj, const TypedRef& out) const
	{
		if (!_getter)
			return std::unexpected(FieldError{FieldError::NotReadable});

		return _getter(obj, out);
	}

	std::expected<void, FieldError> FieldInfo::SetValue(void* obj, const TypedRef& in) const
	{
		if (!_setter)
			return std::unexpected(FieldError{FieldError::NotWritable});

		return _setter(obj, in);
	}

	std::expected<TypedRef, FieldError> FieldInfo::GetRef(void* obj) const
	{
		if (!_referencer)
			return std::unexpected(FieldError{FieldError::NotAddressable});

		return _referencer(obj);
	}

	std::expected<TypedRef, FieldError> FieldInfo::GetRef(const void* obj) const
	{
		if (!_referencer)
			return std::unexpected(FieldError{FieldError::NotAddressable});

		const TypedRef ref = _referencer(const_cast<void*>(obj));
		return TypedRef{.Type = ref.Type, .Data = ref.Data, .IsConst = true};
	}
}
