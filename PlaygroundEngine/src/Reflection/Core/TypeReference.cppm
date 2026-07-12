export module PlaygroundEngine.Reflection.Core:TypeReference;

namespace PgE
{
	export class TypeInfo;

	export struct TypeReference
	{
		// A cross-reference from one reflected entity to another type's TypeInfo, resolved on demand.
		// It stores the resolver's address, not the TypeInfo pointer, so the referenced type need not be
		// complete when the reference is built, only when it is first read at runtime. That is what lets a
		// type's metadata name itself (a factory method returning the type by value) or two types name each
		// other, without the consteval construction of one depending on the completion of the other.
		//
		// The resolver returns the one program-lifetime TypeInfo instance for its type, so &Get() yields the
		// same pointer identity that &TypeOf<T>() does: annotation matching, serialization, and C# dedup keep
		// comparing by that pointer as before.

		using Resolver = const TypeInfo& (*)();

		Resolver Resolve = nullptr;

		const TypeInfo& Get() const { return Resolve(); }
	};
}
