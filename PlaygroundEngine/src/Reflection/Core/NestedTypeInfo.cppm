export module PlaygroundEngine.Reflection.Core:NestedTypeInfo;

import :TypeReference;
import :DeclarationInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	// A type declared or aliased inside another type: a nested class, enum, or union, or a member type-alias.
	// The name is the member's (Config, ValueType); the reference resolves to the underlying type's TypeInfo,
	// so an alias points at what it aliases. It is the reflexive structure a C# nested-type projection mirrors.
	export class NestedTypeInfo : public DeclarationInfo
	{
	public:
		constexpr NestedTypeInfo(const TypeReference type,
								 const std::string_view identifier,
								 const std::string_view displayName,
								 const std::span<const std::string_view> scopePath,
								 const AccessKind access,
								 const bool isAlias,
								 const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _type(type), _access(access), _isAlias(isAlias)
		{}

		const TypeInfo& GetTypeInfo() const
		{
			return _type.Get();
		}

		AccessKind GetAccess() const
		{
			return _access;
		}

		// A member type-alias (using/typedef) rather than a nested type definition. Both are reflected; this
		// separates a nested struct Config from using ValueType = int, which a projection renders differently.
		bool IsAlias() const
		{
			return _isAlias;
		}

	private:
		TypeReference _type;
		AccessKind _access = AccessKind::Public;
		bool _isAlias = false;
	};
}
