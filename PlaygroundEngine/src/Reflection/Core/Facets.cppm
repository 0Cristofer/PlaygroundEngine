export module PlaygroundEngine.Reflection.Core:Facets;

import :TypeReference;
import :TypedRef;

import std;

namespace PgE
{
	export struct FacetEntry
	{
		TypeReference Type;
		const void* Data = nullptr;
	};

	export struct FacetError
	{
		enum Kind : std::uint8_t
		{
			NotWritable,
			TypeMismatch,
			ConstViolation,

			// The object is not the type that provides this facet, so its thunks would index another layout.
			ObjectTypeMismatch,

			NullObject,
		};

		Kind Reason;
	};

	// A facet is reached through its owning TypeInfo, so a mismatch means a caller cached one and applied it
	// elsewhere. An empty owner skips the check, which is what a facet built outside the builder has.
	export std::expected<void, FacetError> CheckFacetOwner(TypeReference owner, const TypedRef& object);
}
