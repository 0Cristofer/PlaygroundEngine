export module PlaygroundEngine.Reflection:TypedRef;

namespace PgE
{
	export class TypeInfo;

	export struct TypedRef
	{
		const TypeInfo* const Type = nullptr;
		void* const Data = nullptr;
		const bool IsConst = false;
	};
}
