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

		std::size_t Size(const void* obj) const pre(_size != nullptr)
		{
			return _size(obj);
		}

		TypedRef ElementRef(void* obj, const std::size_t index) const pre(_elementRef != nullptr)
		{
			// _elementRef is null for a read-only view (std::span<const T>), so this pre holds only for a
			// mutable sequence; a caller that mutates gates on CanMutateElements() first, as CanAppend gates Append.
			return _elementRef(obj, index);
		}

		TypedRef ElementRef(const void* obj, const std::size_t index) const pre(_constElementRef != nullptr)
		{
			return _constElementRef(obj, index);
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

		std::expected<void, FacetError> Clear(void* obj) const
		{
			if (!_clear)
			{
				return std::unexpected(FacetError{FacetError::NotWritable});
			}
			_clear(obj);
			return {};
		}

		std::expected<void, FacetError> Reserve(void* obj, const std::size_t capacity) const
		{
			if (!_reserve)
			{
				return std::unexpected(FacetError{FacetError::NotWritable});
			}
			_reserve(obj, capacity);
			return {};
		}

		std::expected<void, FacetError> Append(void* obj, const TypedRef& in) const
		{
			if (!_append)
			{
				return std::unexpected(FacetError{FacetError::NotWritable});
			}
			return _append(obj, in);
		}

	private:
		TypeReference _element;
		SizeThunk _size = nullptr;
		ElementRefThunk _elementRef = nullptr;
		ConstElementRefThunk _constElementRef = nullptr;
		ClearThunk _clear = nullptr;
		ReserveThunk _reserve = nullptr;
		AppendThunk _append = nullptr;
	};
}
