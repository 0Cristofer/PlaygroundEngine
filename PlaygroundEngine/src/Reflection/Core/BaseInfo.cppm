export module PlaygroundEngine.Reflection.Core:BaseInfo;

import :TypeReference;
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
						   const AccessKind access,
						   const std::size_t offset,
						   const std::span<const AnnotationInfo> annotations)
			: _type(type), _access(access), _offset(offset), _annotations(annotations)
		{}

		[[nodiscard]] const TypeInfo& GetTypeInfo() const
		{
			return _type.Get();
		}

		[[nodiscard]] AccessKind GetAccess() const
		{
			return _access;
		}

		[[nodiscard]] std::size_t GetOffset() const
		{
			return _offset;
		}

		[[nodiscard]] std::span<const AnnotationInfo> GetAnnotations() const
		{
			return _annotations;
		}

	private:
		TypeReference _type;
		AccessKind _access = AccessKind::Public;
		std::size_t _offset = 0;
		std::span<const AnnotationInfo> _annotations;
	};
}
