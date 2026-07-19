export module PlaygroundEngine.Reflection.Core:FunctionSignatureInfo;

import :TypeReference;

import std;

namespace PgE
{
	export class TypeInfo;

	export class FunctionSignatureInfo
	{
		// The call shape of a function type (TypeKind::Function), which is what a function-pointer field, a
		// callback parameter, or a std::function template argument bottoms out on. Parameters are bare type
		// references: a function type has no parameter names, default arguments, or annotations to carry.

	public:
		constexpr FunctionSignatureInfo(const TypeReference returnType,
										const std::span<const TypeReference> parameters,
										const bool isNoexcept,
										const bool isVariadic)
			: _returnType(returnType), _parameters(parameters), _isNoexcept(isNoexcept), _isVariadic(isVariadic)
		{}

		const TypeInfo& GetReturnType() const
		{
			return _returnType.Get();
		}

		// The parameter types as written, undecayed: a signature's whole content is the exact types, and a
		// reference or cv-qualified type has a TypeInfo of its own through the compound-decomposition chain.
		std::span<const TypeReference> GetParameters() const
		{
			return _parameters;
		}

		bool IsNoexcept() const
		{
			return _isNoexcept;
		}

		// A C-style trailing ellipsis. std::meta has no query for it, so it is detected structurally, which
		// does not reach an abominable function type. See docs/ReflectionExtraction.md (function types).
		bool IsVariadic() const
		{
			return _isVariadic;
		}

	private:
		TypeReference _returnType;
		std::span<const TypeReference> _parameters;
		bool _isNoexcept = false;
		bool _isVariadic = false;
	};
}
