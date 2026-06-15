export module PlaygroundEngine.Reflection.TypeInfo:Field;

import std;

namespace PlaygroundEngine
{
	export class TypeInfo;

	export class Field
	{
	public:
		constexpr Field(const TypeInfo* typeInfo, const std::string_view _displayName, const int byteOffset, const int bitOffset) :
			_typeInfo(typeInfo), _name(_displayName), _byteOffset(byteOffset), _bitOffset(bitOffset)
		{
		}

		int GetByteOffset() const;
		std::string_view GetName() const;
		const TypeInfo& GetTypeInfo() const;

	private:
		const TypeInfo* _typeInfo;
		std::string_view _name;
		int _byteOffset;
		int _bitOffset;
	};
}
