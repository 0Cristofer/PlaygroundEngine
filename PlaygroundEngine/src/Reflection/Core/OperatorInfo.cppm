export module PlaygroundEngine.Reflection.Core:OperatorInfo;

import :FunctionInfo;
import :TypeReference;
import :DeclarationInfo;
import :ParameterInfo;
import :TypedRef;

import std;

namespace PgE
{
	export enum class OperatorKind : std::uint8_t
	{
		// The full set of overloadable operators, so a reflected operator function is always nameable by kind
		// rather than by an implementation-defined spelling. Unknown is the total-switch fallback only.
		Unknown,

		New,
		Delete,
		ArrayNew,
		ArrayDelete,
		CoAwait,

		Call,
		Subscript,
		Arrow,
		ArrowStar,

		BitwiseNot,
		LogicalNot,

		Plus,
		Minus,
		Multiply,
		Divide,
		Modulo,
		BitwiseXor,
		BitwiseAnd,
		BitwiseOr,

		Assign,
		PlusAssign,
		MinusAssign,
		MultiplyAssign,
		DivideAssign,
		ModuloAssign,
		BitwiseXorAssign,
		BitwiseAndAssign,
		BitwiseOrAssign,
		LeftShiftAssign,
		RightShiftAssign,

		Equal,
		NotEqual,
		Less,
		Greater,
		LessEqual,
		GreaterEqual,
		Compare,

		LogicalAnd,
		LogicalOr,
		LeftShift,
		RightShift,

		Increment,
		Decrement,
		Comma,
	};

	// An overloaded operator: a FunctionInfo (return type, parameters, traits, invoker) plus the operator it
	// spells. It is a distinct list from GetFunctions so a consumer keys on the kind rather than parsing a
	// name, and projects it the way a target runtime expects (C# op_Equality, op_Addition, ...).
	export class OperatorInfo : public FunctionInfo
	{
	public:
		constexpr OperatorInfo(const OperatorKind kind,
							   const TypeReference returnType,
							   const std::string_view identifier,
							   const std::string_view displayName,
							   const std::span<const std::string_view> scopePath,
							   const std::span<const ParameterInfo> params,
							   const FunctionTraits& traits,
							   const Invoker invoke,
							   const std::span<const AnnotationInfo> annotations)
			: FunctionInfo(returnType, identifier, displayName, scopePath, params, traits, invoke, annotations), _kind(kind)
		{}

		OperatorKind GetOperator() const
		{
			return _kind;
		}

	private:
		OperatorKind _kind = OperatorKind::Unknown;
	};
}
