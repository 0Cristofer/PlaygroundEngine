module;

export module PlaygroundEngine.Reflection.Builtins:StringFacet;

import PlaygroundEngine.Reflection.Core;

import std;

namespace PgE
{
	export class StringFacet
	{
		// A type that reads and (optionally) writes as a run of characters: a read-only string (string_view)
		// leaves the assign thunk null, the nullable-capability encoding FieldInfo uses. It supersedes the raw
		// field view. See docs/ReflectionInternals.md (Facets).

	public:
		// Read generically by the builder: a facet declaring Supersedes = true empties the structural
		// field/function view, since its structure is an implementation detail.
		static constexpr bool Supersedes = true;

		using ViewThunk = std::string_view (*)(const void*);
		using AssignThunk = std::expected<void, FacetError> (*)(void*, std::string_view);

		constexpr StringFacet(const ViewThunk view, const AssignThunk assign) : _view(view), _assign(assign)
		{}

		std::string_view View(const TypedRef& object) const pre(_view != nullptr) pre(CheckFacetOwner(_owner, object).has_value())
		{
			return _view(object.Data);
		}

		bool CanAssign() const
		{
			return _assign != nullptr;
		}

		std::expected<void, FacetError> Assign(const TypedRef& object, const std::string_view value) const
		{
			if (const auto owned = CheckFacetOwner(_owner, object); !owned)
			{
				return owned;
			}
			if (object.IsConst)
			{
				return std::unexpected(FacetError{FacetError::ConstViolation});
			}
			if (!_assign)
			{
				return std::unexpected(FacetError{FacetError::NotWritable});
			}
			return _assign(object.Data, value);
		}

		// Set by the facet builder to the type that provides this facet, so an op can tell an object of that
		// type from any other. Empty for a facet built by hand, which skips the check.
		constexpr void SetOwnerType(const TypeReference owner)
		{
			_owner = owner;
		}

	private:
		TypeReference _owner;
		ViewThunk _view = nullptr;
		AssignThunk _assign = nullptr;
	};
}
