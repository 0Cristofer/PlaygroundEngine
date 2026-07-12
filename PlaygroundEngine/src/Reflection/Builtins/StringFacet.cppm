module;

#include <cassert>

export module PlaygroundEngine.Reflection.Builtins:StringFacet;

import PlaygroundEngine.Reflection.Core;

import std;

namespace PgE
{
	export class StringFacet
	{
		// A type that reads and (optionally) writes as a run of characters. The read view is always present;
		// a read-only string (a string_view) leaves the assign thunk null, the same nullable-capability
		// encoding FieldInfo uses. Its character run is the whole value, so this supersedes the raw field view
		// (see Supersedes);

	public:
		// Read generically by the builder: a facet declaring Supersedes = true empties the structural
		// field/function view, since its structure is an implementation detail.
		static constexpr bool Supersedes = true;

		using ViewThunk = std::string_view (*)(const void*);
		using AssignThunk = std::expected<void, FacetError> (*)(void*, std::string_view);

		constexpr StringFacet(const ViewThunk view, const AssignThunk assign) : _view(view), _assign(assign)
		{
		}

		std::string_view View(const void* obj) const
		{
			assert(_view && "StringFacet is missing its view read thunk");
			return _view(obj);
		}

		bool CanAssign() const { return _assign != nullptr; }

		std::expected<void, FacetError> Assign(void* obj, const std::string_view value) const
		{
			if (!_assign)
				return std::unexpected(FacetError{FacetError::NotWritable});
			return _assign(obj, value);
		}

	private:
		ViewThunk _view = nullptr;
		AssignThunk _assign = nullptr;
	};
}
