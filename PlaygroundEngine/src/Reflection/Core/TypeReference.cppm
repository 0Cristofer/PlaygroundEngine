export module PlaygroundEngine.Reflection.Core:TypeReference;

namespace PgE
{
	export class TypeInfo;

	export struct TypeReference
	{
		// A cross-reference to another type's TypeInfo, resolved on demand: it stores the resolver's address,
		// not a TypeInfo pointer, so the referenced type need not be complete when the reference is built.
		// See docs/ReflectionInternals.md (TypeReference lazy cross-references).

		using Resolver = const TypeInfo& (*)();

		Resolver Resolve = nullptr;

		const TypeInfo& Get() const
		{
			return Resolve();
		}
	};
}
