module;

export module PlaygroundEngine.Reflection.Builtins:SequenceFacet;

import PlaygroundEngine.Reflection.Core;

import std;

namespace PgE
{
	export class SequenceFacet
	{
		// An ordered, random-access run of elements; capacity mutation and element mutation each go through
		// thunks gated by CanX queries, so fixed-size and read-only views report their limits. It supersedes
		// the structural field view. See docs/ReflectionInternals.md (Facets).

	public:
		// Read generically by the builder: a facet declaring Supersedes = true empties the structural
		// field/function view, since its structure is an implementation detail.
		static constexpr bool Supersedes = true;

		using SizeThunk = std::size_t (*)(const void*);
		using ElementRefThunk = TypedRef (*)(void*, std::size_t);
		using ConstElementRefThunk = TypedRef (*)(const void*, std::size_t);
		using ClearThunk = void (*)(void*);
		using ReserveThunk = void (*)(void*, std::size_t);
		using AppendThunk = std::expected<void, FacetError> (*)(void*, const TypedRef&);

		constexpr SequenceFacet(const TypeReference element,
								const SizeThunk size,
								const ElementRefThunk elementRef,
								const ConstElementRefThunk constElementRef,
								const ClearThunk clear,
								const ReserveThunk reserve,
								const AppendThunk append)
			: _element(element), _size(size), _elementRef(elementRef), _constElementRef(constElementRef), _clear(clear), _reserve(reserve),
			  _append(append)
		{}

		const TypeInfo& ElementType() const
		{
			return _element.Get();
		}

		std::size_t Size(const TypedRef& object) const pre(_size != nullptr) pre(CheckFacetOwner(_owner, object).has_value())
		{
			return _size(object.Data);
		}

		TypedRef ElementRef(const TypedRef& object, const std::size_t index) const pre(_constElementRef != nullptr)
			pre(CheckFacetOwner(_owner, object).has_value())
		{
			if (object.IsConst || _elementRef == nullptr)
			{
				return _constElementRef(object.Data, index);
			}

			return _elementRef(object.Data, index);
		}

		bool CanMutateElements() const
		{
			return _elementRef != nullptr;
		}
		bool CanClear() const
		{
			return _clear != nullptr;
		}
		bool CanReserve() const
		{
			return _reserve != nullptr;
		}
		bool CanAppend() const
		{
			return _append != nullptr;
		}

		std::expected<void, FacetError> Clear(const TypedRef& object) const
		{
			if (const auto owned = CheckFacetOwner(_owner, object); !owned)
			{
				return owned;
			}
			if (object.IsConst)
			{
				return std::unexpected(FacetError{FacetError::ConstViolation});
			}
			if (!_clear)
			{
				return std::unexpected(FacetError{FacetError::NotWritable});
			}
			_clear(object.Data);
			return {};
		}

		std::expected<void, FacetError> Reserve(const TypedRef& object, const std::size_t capacity) const
		{
			if (const auto owned = CheckFacetOwner(_owner, object); !owned)
			{
				return owned;
			}
			if (object.IsConst)
			{
				return std::unexpected(FacetError{FacetError::ConstViolation});
			}
			if (!_reserve)
			{
				return std::unexpected(FacetError{FacetError::NotWritable});
			}
			_reserve(object.Data, capacity);
			return {};
		}

		std::expected<void, FacetError> Append(const TypedRef& object, const TypedRef& in) const
		{
			if (const auto owned = CheckFacetOwner(_owner, object); !owned)
			{
				return owned;
			}
			if (object.IsConst)
			{
				return std::unexpected(FacetError{FacetError::ConstViolation});
			}
			if (!_append)
			{
				return std::unexpected(FacetError{FacetError::NotWritable});
			}
			return _append(object.Data, in);
		}

		// Set by the facet builder to the type that provides this facet, so an op can tell an object of that
		// type from any other. Empty for a facet built by hand, which skips the check.
		constexpr void SetOwnerType(const TypeReference owner)
		{
			_owner = owner;
		}

	private:
		TypeReference _owner;
		TypeReference _element;
		SizeThunk _size = nullptr;
		ElementRefThunk _elementRef = nullptr;
		ConstElementRefThunk _constElementRef = nullptr;
		ClearThunk _clear = nullptr;
		ReserveThunk _reserve = nullptr;
		AppendThunk _append = nullptr;
	};
}
