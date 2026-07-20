export module PlaygroundEngine.Reflection.Core:NamespaceInfo;

import :DeclarationInfo;
import :NestedTypeInfo;

import std;

namespace PgE
{
	export class FunctionInfo;
	export class StaticFieldInfo;

	// A namespace and the entities declared directly in it. What a namespace contains is a property of where
	// the question is asked, not of the entity: the members are reported as found, and reconciling two sweeps
	// that disagree is the consumer's. See docs/ReflectionExtraction.md (namespace sweep).
	export class NamespaceInfo : public DeclarationInfo
	{
	public:
		constexpr NamespaceInfo(const std::string_view identifier,
								const std::string_view displayName,
								const std::span<const std::string_view> scopePath,
								const std::span<const AnnotationInfo> annotations,
								const std::span<const NestedTypeInfo> types,
								const std::span<const FunctionInfo* const> functions,
								const std::span<const StaticFieldInfo* const> variables,
								const std::span<const NamespaceInfo* const> namespaces)
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _types(types), _functions(functions), _variables(variables),
			  _namespaces(namespaces)
		{}

		// Each carries the name it was declared under, so an alias is told from the type it names. Same shape
		// as a type's own nested types, which is why it is the same metadata.
		std::span<const NestedTypeInfo> GetTypes() const
		{
			return _types;
		}

		// Pointers, not values: these are the objects FunctionMetaOf and VariableMetaOf hand out, so an entity
		// reached by naming it compares equal to the one found by sweeping.
		std::span<const FunctionInfo* const> GetFunctions() const
		{
			return _functions;
		}

		std::span<const StaticFieldInfo* const> GetVariables() const
		{
			return _variables;
		}

		// An inline namespace is reported as an ordinary nested one, and its members stay inside it rather than
		// being lifted here the way name lookup lifts them. See docs/ReflectionExtraction.md.
		std::span<const NamespaceInfo* const> GetNamespaces() const
		{
			return _namespaces;
		}

	private:
		std::span<const NestedTypeInfo> _types;
		std::span<const FunctionInfo* const> _functions;
		std::span<const StaticFieldInfo* const> _variables;
		std::span<const NamespaceInfo* const> _namespaces;
	};
}
