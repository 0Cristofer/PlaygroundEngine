module PlaygroundEngine.Reflection.TypeInfo;

namespace PlaygroundEngine
{
	int Field::GetByteOffset() const
	{
		return _byteOffset;
	}

	std::string_view Field::GetName() const
	{
		return _name;
	}

	const TypeInfo& Field::GetTypeInfo() const
	{
		return *_typeInfo;
	}
}
