export module PlaygroundEngine.Reflection.Core:MemberPointerInfo;

import :TypeReference;

import std;

namespace PgE
{
	export class TypeInfo;

	export class MemberPointerInfo
	{
		// The two halves of a member pointer: int Foo::* is Foo plus int, and int (Foo::*)(double) const is
		// Foo plus a function type whose own signature carries the rest. remove_pointer does not peel a member
		// pointer, so this is a separate decomposition from the single inner-type chain.

	public:
		constexpr MemberPointerInfo(const TypeReference classType, const TypeReference pointeeType) : _classType(classType), _pointeeType(pointeeType)
		{}

		const TypeInfo& GetClassType() const
		{
			return _classType.Get();
		}

		const TypeInfo& GetPointeeType() const
		{
			return _pointeeType.Get();
		}

	private:
		TypeReference _classType;
		TypeReference _pointeeType;
	};
}
