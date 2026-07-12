module;

#include <meta>

export module PlaygroundEngine.Reflection.Builtins:SequenceFacetBuilder;

import PlaygroundEngine.Reflection.Core;

import :SequenceFacet;

import std;

// The pre-made sequence facet and its TypeInfoTraits specializations (std::vector, std::array, and a C
// array T[N]).

namespace PgE
{
	namespace detail
	{
		template <typename Container>
		std::size_t SequenceSizeThunk(const void* obj)
		{
			return static_cast<const Container*>(obj)->size();
		}

		// The element type's reflection is a template parameter, not a `^^Element` spelled in the body: the
		// reflection operator is consteval-only and cannot appear in a runtime thunk. The tag is resolved
		// into a local before the TypedRef literal, since GCC flags a TypeOfMeta call inside a
		// designated-initializer list as a consteval-only expression in a runtime context.
		template <typename Container, std::meta::info ElementMeta>
		TypedRef SequenceElementRefThunk(void* obj, const std::size_t index)
		{
			using Element = [:ElementMeta:];
			const TypeInfo* tag = &TypeOfMeta<ElementMeta>();
			Element& element = (*static_cast<Container*>(obj))[index];
			return TypedRef{.Type = tag, .Data = std::addressof(element), .IsConst = false};
		}

		template <typename Container, std::meta::info ElementMeta>
		TypedRef SequenceElementRefConstThunk(const void* obj, const std::size_t index)
		{
			using Element = [:ElementMeta:];
			const TypeInfo* tag = &TypeOfMeta<ElementMeta>();
			const Element& element = (*static_cast<const Container*>(obj))[index];
			return TypedRef{
				.Type = tag,
				.Data = const_cast<void*>(static_cast<const void*>(std::addressof(element))),
				.IsConst = true,
			};
		}

		template <std::size_t Count>
		std::size_t CArraySizeThunk(const void*)
		{
			return Count;
		}

		template <std::meta::info ElementMeta>
		TypedRef CArrayElementRefThunk(void* obj, const std::size_t index)
		{
			using Element = [:ElementMeta:];
			const TypeInfo* tag = &TypeOfMeta<ElementMeta>();
			Element* base = static_cast<Element*>(obj);
			return TypedRef{.Type = tag, .Data = std::addressof(base[index]), .IsConst = false};
		}

		template <std::meta::info ElementMeta>
		TypedRef CArrayElementRefConstThunk(const void* obj, const std::size_t index)
		{
			using Element = [:ElementMeta:];
			const TypeInfo* tag = &TypeOfMeta<ElementMeta>();
			const Element* base = static_cast<const Element*>(obj);
			return TypedRef{
				.Type = tag,
				.Data = const_cast<void*>(static_cast<const void*>(std::addressof(base[index]))),
				.IsConst = true,
			};
		}

		template <typename Container>
		void SequenceClearThunk(void* obj)
		{
			static_cast<Container*>(obj)->clear();
		}

		template <typename Container>
		void SequenceReserveThunk(void* obj, const std::size_t capacity)
		{
			static_cast<Container*>(obj)->reserve(capacity);
		}

		template <typename Container, std::meta::info ElementMeta>
		std::expected<void, FacetError> SequenceAppendThunk(void* obj, const TypedRef& in)
		{
			using Element = [:ElementMeta:];
			if (in.Type != &TypeOfMeta<ElementMeta>())
			{
				return std::unexpected(FacetError{FacetError::TypeMismatch});
			}

			Container& container = *static_cast<Container*>(obj);
			Element* source = static_cast<Element*>(in.Data);

			// Same move protocol as FieldSetThunk: move when the caller opted in from a mutable source and
			// the element moves, copy otherwise, NotWritable when the element is neither movable nor copyable.
			const bool moveAllowed = in.Movable && !in.IsConst;
			if constexpr (std::is_move_constructible_v<Element> && std::is_copy_constructible_v<Element>)
			{
				if (moveAllowed)
				{
					container.push_back(std::move(*source));
				}
				else
				{
					container.push_back(*source);
				}
			}
			else if constexpr (std::is_move_constructible_v<Element>)
			{
				if (!moveAllowed)
				{
					return std::unexpected(FacetError{FacetError::NotWritable});
				}
				container.push_back(std::move(*source));
			}
			else if constexpr (std::is_copy_constructible_v<Element>)
			{
				container.push_back(*source);
			}
			else
			{
				return std::unexpected(FacetError{FacetError::NotWritable});
			}

			return {};
		}

		template <typename Container>
		consteval SequenceFacet MakeResizableSequenceFacet()
		{
			using Element = Container::value_type;

			// A non-movable, non-copyable element cannot be appended or reserved for (both operations relocate
			// elements), so those thunks stay null and the sequence is reachable only through the element refs.
			// The branch also keeps SequenceReserveThunk/SequenceAppendThunk from instantiating for such an
			// element, where reserve's MoveInsertable requirement would be a hard error. Documented, not one.
			if constexpr (std::is_move_constructible_v<Element> || std::is_copy_constructible_v<Element>)
			{
				return SequenceFacet{
					TypeReferenceTo<^^Element>(),
					&SequenceSizeThunk<Container>,
					&SequenceElementRefThunk<Container, ^^Element>,
					&SequenceElementRefConstThunk<Container, ^^Element>,
					&SequenceClearThunk<Container>,
					&SequenceReserveThunk<Container>,
					&SequenceAppendThunk<Container, ^^Element>,
				};
			}
			else
			{
				return SequenceFacet{
					TypeReferenceTo<^^Element>(),
					&SequenceSizeThunk<Container>,
					&SequenceElementRefThunk<Container, ^^Element>,
					&SequenceElementRefConstThunk<Container, ^^Element>,
					&SequenceClearThunk<Container>,
					nullptr,
					nullptr,
				};
			}
		}

		template <typename Container>
		consteval SequenceFacet MakeFixedSequenceFacet()
		{
			// A fixed-size, `.size()`/`operator[]`-indexable container (std::array, std::span): element access with
			// no resize. It carries any type whose elements are reached by subscript, owning or a view.

			using Value = Container::value_type;
			using Element = std::remove_cv_t<Value>;

			// A read-only view (std::span<const T>) yields const references from operator[], so it exposes no
			// mutable element ref. The branch also keeps SequenceElementRefThunk from instantiating for such a
			// container, where binding a non-const Element& to a const element would be a hard error.
			using ElementReference = decltype(std::declval<Container&>()[std::size_t{0}]);
			if constexpr (std::is_const_v<std::remove_reference_t<ElementReference>>)
			{
				return SequenceFacet{
					TypeReferenceTo<^^Element>(),
					&SequenceSizeThunk<Container>,
					nullptr,
					&SequenceElementRefConstThunk<Container, ^^Element>,
					nullptr,
					nullptr,
					nullptr,
				};
			}
			else
			{
				return SequenceFacet{
					TypeReferenceTo<^^Element>(),
					&SequenceSizeThunk<Container>,
					&SequenceElementRefThunk<Container, ^^Element>,
					&SequenceElementRefConstThunk<Container, ^^Element>,
					nullptr,
					nullptr,
					nullptr,
				};
			}
		}

		template <typename Container, typename Element>
		std::string StringifySequence(const Container& value)
		{
			constexpr std::size_t longSequenceThreshold = 10;
			const std::size_t count = std::size(value);
			const bool isLongSequence = count > longSequenceThreshold;
			const std::size_t elementsToRender = isLongSequence ? longSequenceThreshold : count;

			std::string out = "[";
			std::size_t index = 0;
			for (const Element& element : value)
			{
				if (index >= elementsToRender)
				{
					break;
				}
				if (index != 0)
				{
					out += ", ";
				}
				out += ToString(element);
				++index;
			}
			if (isLongSequence)
			{
				out += ", ...";
			}
			return out + "]";
		}
	}

	template <typename Element, typename Alloc>
	struct TypeInfoTraits<std::vector<Element, Alloc>> : TypeInfoTraitsDefaults
	{
		// A proxy-reference container (std::vector<bool>) is intentionally not matched: the element thunks bind
		// a real Element& to operator[], which a proxy rvalue cannot satisfy. Bit-packed data has no place in
		// reflected structures, so leaving it to fail at reflection time rather than special-casing it is fine.

		static std::string Stringify(const std::vector<Element, Alloc>& value)
		{
			return detail::StringifySequence<std::vector<Element, Alloc>, Element>(value);
		}

		static consteval auto MakeFacets() { return std::tuple{detail::MakeResizableSequenceFacet<std::vector<Element, Alloc>>()}; }
	};

	template <typename Element, std::size_t Count>
	struct TypeInfoTraits<std::array<Element, Count>> : TypeInfoTraitsDefaults
	{
		// std::array is fixed size: clear/reserve/append stay null, mutation goes through the element refs.

		static std::string Stringify(const std::array<Element, Count>& value)
		{
			return detail::StringifySequence<std::array<Element, Count>, Element>(value);
		}

		static consteval auto MakeFacets() { return std::tuple{detail::MakeFixedSequenceFacet<std::array<Element, Count>>()}; }
	};

	template <typename Element, std::size_t Count>
	struct TypeInfoTraits<Element[Count]> : TypeInfoTraitsDefaults
	{
		static std::string Stringify(const Element (&value)[Count]) { return detail::StringifySequence<Element[Count], Element>(value); }

		static consteval auto MakeFacets()
		{
			return std::tuple{SequenceFacet{
				detail::TypeReferenceTo<^^Element>(),
				&detail::CArraySizeThunk<Count>,
				&detail::CArrayElementRefThunk<^^Element>,
				&detail::CArrayElementRefConstThunk<^^Element>,
				nullptr,
				nullptr,
				nullptr,
			}};
		}
	};

	template <typename Element, std::size_t Extent>
	struct TypeInfoTraits<std::span<Element, Extent>> : TypeInfoTraitsDefaults
	{
		// A non-owning view over a contiguous run, of either extent: like std::array it exposes size and element
		// refs with no resize, but it never owns its elements, so a serializer treats it as a borrow rather than
		// inline data (the facet only states the in-place read/mutate capability, not ownership). A const-element
		// view (std::span<const T>) is read-only: MakeFixedSequenceFacet detects the const references from
		// operator[] and omits the mutable element ref, so CanMutateElements() is false while reads still work.

		static std::string Stringify(const std::span<Element, Extent>& value)
		{
			return detail::StringifySequence<std::span<Element, Extent>, Element>(value);
		}

		static consteval auto MakeFacets() { return std::tuple{detail::MakeFixedSequenceFacet<std::span<Element, Extent>>()}; }
	};
}
