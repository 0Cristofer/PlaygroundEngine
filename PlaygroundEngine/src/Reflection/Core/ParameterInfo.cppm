export module PlaygroundEngine.Reflection.Core:ParameterInfo;

import :TypeReference;
import :DeclarationInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export struct ParameterTraits
	{
		// The language facts of one parameter, beside its decayed type. Qualifiers are the language's own
		// spelling; BindsByMove / RequiresMutable are the binder's policy derived from them, kept distinct
		// so a consumer reads the fact and the erased-call path reads the policy.

		bool IsConst = false;
		bool IsLvalueReference = false;
		bool IsRvalueReference = false;

		// A default argument must be reproduced by the C# generator and lets a visual-scripting node omit
		// the pin, so its absence is a hard blocker for both.
		bool HasDefaultArgument = false;

		bool BindsByMove = false;
		bool RequiresMutable = false;
	};

	export class ParameterInfo : public DeclarationInfo
	{
		// One parameter of a callable (function or constructor): its type plus the shared declaration
		// metadata. Shared vocabulary so neither FunctionInfo nor ConstructorInfo owns it.

	public:
		constexpr ParameterInfo(const TypeReference typeInfo,
								const std::string_view identifier,
								const std::string_view displayName,
								const std::span<const std::string_view> scopePath,
								const ParameterTraits& traits,
								const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _typeInfo(typeInfo), _traits(traits)
		{}

		// The parameter's type, stripped of its reference and const qualifiers: what an erased argument is
		// tagged with. The traits carry what stripping them loses, so const std::string& and std::string&&
		// stay distinguishable despite sharing one tag.
		const TypeInfo& GetTypeInfo() const
		{
			return _typeInfo.Get();
		}

		const ParameterTraits& GetTraits() const
		{
			return _traits;
		}
		bool IsConst() const
		{
			return _traits.IsConst;
		}
		bool IsLvalueReference() const
		{
			return _traits.IsLvalueReference;
		}
		bool IsRvalueReference() const
		{
			return _traits.IsRvalueReference;
		}
		bool HasDefaultArgument() const
		{
			return _traits.HasDefaultArgument;
		}

		// Binding this parameter moves out of the argument, so the caller must offer it (an rvalue-ref
		// parameter, or a by-value one of a move-only type).
		bool BindsByMove() const
		{
			return _traits.BindsByMove;
		}

		bool RequiresMutable() const
		{
			return _traits.RequiresMutable;
		}

	private:
		TypeReference _typeInfo;
		ParameterTraits _traits;
	};
}
