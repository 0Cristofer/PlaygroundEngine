export module PlaygroundEngine.Reflection.Core:ConversionInfo;

import :FunctionInfo;
import :TypeReference;
import :DeclarationInfo;
import :ParameterInfo;
import :TypedRef;

import std;

namespace PgE
{
	export class TypeInfo;

	// A user-defined conversion function (operator T()): a FunctionInfo whose return type is the conversion
	// target, plus the explicit flag the target runtime needs (an explicit conversion projects differently
	// from an implicit one). Kept off GetFunctions, where it would appear nameless, and modelled here instead.
	export class ConversionInfo : public FunctionInfo
	{
	public:
		constexpr ConversionInfo(const bool isExplicit,
								 const TypeReference targetType,
								 const std::string_view displayName,
								 const std::span<const std::string_view> scopePath,
								 const FunctionTraits& traits,
								 const Invoker invoke,
								 const std::span<const AnnotationInfo> annotations)
			: FunctionInfo(targetType, {}, displayName, scopePath, {}, traits, invoke, annotations), _isExplicit(isExplicit)
		{}

		// The type this converts to. A conversion's return type is its target, carried with the same
		// decayed-plus-traits shape as any return (GetReturnType plus the ReturnIs* trait flags).
		const TypeInfo& GetTargetType() const
		{
			return GetReturnType();
		}

		bool IsExplicit() const
		{
			return _isExplicit;
		}

	private:
		bool _isExplicit = false;
	};
}
