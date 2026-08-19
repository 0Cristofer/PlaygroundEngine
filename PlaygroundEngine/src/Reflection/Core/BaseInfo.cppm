export module PlaygroundEngine.Reflection.Core:BaseInfo;

import :TypeReference;
import :TypedRef;
import :DeclarationInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export class BaseInfo
	{
		// A direct base class of a reflected type. Offset is a true layout constant (virtual inheritance is
		// rejected in the bases builder), so base offsets chain, which a later Cast<T> relies on. See
		// docs/ReflectionInternals.md (type hierarchy).
	public:
		constexpr BaseInfo(const TypeReference type,
						   const TypeReference derivedType,
						   const AccessKind access,
						   const std::size_t offset,
						   const std::span<const AnnotationInfo> annotations)
			: _type(type), _derivedType(derivedType), _access(access), _offset(offset), _annotations(annotations)
		{}

		[[nodiscard]] const TypeInfo& GetTypeInfo() const
		{
			return _type.Get();
		}

		// The type this base is a base OF, so an upcast can verify it was handed the right object.
		[[nodiscard]] const TypeInfo& GetDerivedType() const
		{
			return _derivedType.Get();
		}

		[[nodiscard]] AccessKind GetAccess() const
		{
			return _access;
		}

		[[nodiscard]] std::size_t GetOffset() const
		{
			return _offset;
		}

		// The base subobject inside a derived object: every member thunk is built against its declaring type,
		// so an inherited member is reached through this borrow, not the derived one. One direct base per
		// hop, which is what stays unambiguous under repeated non-virtual bases.
		[[nodiscard]] TypedRef Upcast(const TypedRef& derived) const pre(derived.Type == &GetDerivedType()) pre(derived.Data != nullptr)
		{
			return TypedRef{
				.Type = &GetTypeInfo(), .Data = static_cast<std::byte*>(derived.Data) + _offset, .IsConst = derived.IsConst, .Movable = false};
		}

		[[nodiscard]] std::span<const AnnotationInfo> GetAnnotations() const
		{
			return _annotations;
		}

	private:
		TypeReference _type;
		TypeReference _derivedType;
		AccessKind _access = AccessKind::Public;
		std::size_t _offset = 0;
		std::span<const AnnotationInfo> _annotations;
	};
}
