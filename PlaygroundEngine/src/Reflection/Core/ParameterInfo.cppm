export module PlaygroundEngine.Reflection.Core:ParameterInfo;

import :TypeReference;
import :DeclarationInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export class ParameterInfo : public DeclarationInfo
	{
		// One parameter of a callable (function or constructor): its type plus the shared declaration
		// metadata. Shared vocabulary so neither FunctionInfo nor ConstructorInfo owns it.

	public:
		constexpr ParameterInfo(const TypeReference typeInfo,
								const std::string_view identifier,
								const std::string_view displayName,
								const bool bindsByMove,
								const bool requiresMutable,
								const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, annotations), _typeInfo(typeInfo), _bindsByMove(bindsByMove), _requiresMutable(requiresMutable)
		{}

		// The parameter's type, stripped of its reference and const qualifiers: what an erased argument is
		// tagged with. The two flags below carry what stripping them loses.
		const TypeInfo& GetTypeInfo() const
		{
			return _typeInfo.Get();
		}

		// Binding this parameter moves out of the argument, so the caller must offer it (an rvalue-ref
		// parameter, or a by-value one of a move-only type).
		bool BindsByMove() const
		{
			return _bindsByMove;
		}

		bool RequiresMutable() const
		{
			return _requiresMutable;
		}

	private:
		TypeReference _typeInfo;
		bool _bindsByMove = false;
		bool _requiresMutable = false;
	};
}
