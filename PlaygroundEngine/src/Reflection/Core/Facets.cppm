export module PlaygroundEngine.Reflection.Core:Facets;

import :TypeReference;

import std;

namespace PgE
{
	export struct FacetEntry
	{
		TypeReference Key;
		const void* Data = nullptr;
	};

	export struct FacetError
	{
		enum Kind : std::uint8_t
		{
			NotWritable,
			TypeMismatch,
		};

		Kind Reason;
	};
}
