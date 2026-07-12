export module PlaygroundEngine.Reflection.Core:TypedRef;

namespace PgE
{
	export class TypeInfo;

	export struct TypedRef
	{
		const TypeInfo* Type = nullptr;
		void* Data = nullptr;
		bool IsConst = false;
		bool Movable = false;
	};
}
