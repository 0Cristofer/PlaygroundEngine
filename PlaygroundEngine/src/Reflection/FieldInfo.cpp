module PlaygroundEngine.Reflection.TypeInfo;

import :FieldInfo;

namespace PlaygroundEngine
{
	int FieldInfo::GetByteOffset() const
	{
		return _byteOffset;
	}

	std::string_view FieldInfo::GetName() const
	{
		return _name;
	}

	const TypeInfo& FieldInfo::GetTypeInfo() const
	{
		return *_typeInfo;
	}
}
