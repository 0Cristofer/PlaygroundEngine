export module PlaygroundEngine.Reflection:FieldInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export class FieldInfo
	{
	public:
		constexpr FieldInfo(const TypeInfo* typeInfo, const std::string_view displayName, const int byteOffset,
		                    const int bitOffset) :
			_typeInfo(typeInfo), _name(displayName), _byteOffset(byteOffset), _bitOffset(bitOffset)
		{
		}

		int GetByteOffset() const;
		std::string_view GetName() const;
		const TypeInfo& GetTypeInfo() const;

	private:
		const TypeInfo* _typeInfo = nullptr;
		std::string_view _name;
		int _byteOffset;
		int _bitOffset;
	};
}
