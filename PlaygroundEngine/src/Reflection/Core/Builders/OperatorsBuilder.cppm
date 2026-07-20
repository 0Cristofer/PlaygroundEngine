module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:OperatorsBuilder;

import :MetaCommon;
import :FacetsBuilder;
import :AnnotationsBuilder;
import :ArgumentBinding;
import :FunctionsBuilder;
import :TypeInfo;
import :OperatorInfo;
import :ParameterInfo;
import :TypedRef;
import :DeclarationInfo;

import std;

// Builds the OperatorInfo list for a type. An operator function reuses the whole function machinery
// (parameter binding, traits, the invoke thunk); this only adds the operator-kind classification and keeps
// operators in their own list instead of GetFunctions.

namespace PgE::detail
{
	consteval std::vector<std::meta::info> GetOperatorFunctions(const std::meta::info type)
	{
		std::vector<std::meta::info> operators;
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			// Copy and move assignment are operator functions but also special members: like the constructors,
			// they belong to the lifetime story, not the value-operator one, so they are excluded here the way
			// GetMemberFunctions excludes them. A user-defined converting assignment (operator=(int)) stays.
			if (std::meta::is_operator_function(member) && !std::meta::is_special_member_function(member))
			{
				operators.push_back(member);
			}
		}
		return operators;
	}

	consteval OperatorKind MapOperatorKind(const std::meta::operators op)
	{
		switch (op)
		{
		case std::meta::operators::op_new:
			return OperatorKind::New;
		case std::meta::operators::op_delete:
			return OperatorKind::Delete;
		case std::meta::operators::op_array_new:
			return OperatorKind::ArrayNew;
		case std::meta::operators::op_array_delete:
			return OperatorKind::ArrayDelete;
		case std::meta::operators::op_co_await:
			return OperatorKind::CoAwait;
		case std::meta::operators::op_parentheses:
			return OperatorKind::Call;
		case std::meta::operators::op_square_brackets:
			return OperatorKind::Subscript;
		case std::meta::operators::op_arrow:
			return OperatorKind::Arrow;
		case std::meta::operators::op_arrow_star:
			return OperatorKind::ArrowStar;
		case std::meta::operators::op_tilde:
			return OperatorKind::BitwiseNot;
		case std::meta::operators::op_exclamation:
			return OperatorKind::LogicalNot;
		case std::meta::operators::op_plus:
			return OperatorKind::Plus;
		case std::meta::operators::op_minus:
			return OperatorKind::Minus;
		case std::meta::operators::op_star:
			return OperatorKind::Multiply;
		case std::meta::operators::op_slash:
			return OperatorKind::Divide;
		case std::meta::operators::op_percent:
			return OperatorKind::Modulo;
		case std::meta::operators::op_caret:
			return OperatorKind::BitwiseXor;
		case std::meta::operators::op_ampersand:
			return OperatorKind::BitwiseAnd;
		case std::meta::operators::op_pipe:
			return OperatorKind::BitwiseOr;
		case std::meta::operators::op_equals:
			return OperatorKind::Assign;
		case std::meta::operators::op_plus_equals:
			return OperatorKind::PlusAssign;
		case std::meta::operators::op_minus_equals:
			return OperatorKind::MinusAssign;
		case std::meta::operators::op_star_equals:
			return OperatorKind::MultiplyAssign;
		case std::meta::operators::op_slash_equals:
			return OperatorKind::DivideAssign;
		case std::meta::operators::op_percent_equals:
			return OperatorKind::ModuloAssign;
		case std::meta::operators::op_caret_equals:
			return OperatorKind::BitwiseXorAssign;
		case std::meta::operators::op_ampersand_equals:
			return OperatorKind::BitwiseAndAssign;
		case std::meta::operators::op_pipe_equals:
			return OperatorKind::BitwiseOrAssign;
		case std::meta::operators::op_less_less_equals:
			return OperatorKind::LeftShiftAssign;
		case std::meta::operators::op_greater_greater_equals:
			return OperatorKind::RightShiftAssign;
		case std::meta::operators::op_equals_equals:
			return OperatorKind::Equal;
		case std::meta::operators::op_exclamation_equals:
			return OperatorKind::NotEqual;
		case std::meta::operators::op_less:
			return OperatorKind::Less;
		case std::meta::operators::op_greater:
			return OperatorKind::Greater;
		case std::meta::operators::op_less_equals:
			return OperatorKind::LessEqual;
		case std::meta::operators::op_greater_equals:
			return OperatorKind::GreaterEqual;
		case std::meta::operators::op_spaceship:
			return OperatorKind::Compare;
		case std::meta::operators::op_ampersand_ampersand:
			return OperatorKind::LogicalAnd;
		case std::meta::operators::op_pipe_pipe:
			return OperatorKind::LogicalOr;
		case std::meta::operators::op_less_less:
			return OperatorKind::LeftShift;
		case std::meta::operators::op_greater_greater:
			return OperatorKind::RightShift;
		case std::meta::operators::op_plus_plus:
			return OperatorKind::Increment;
		case std::meta::operators::op_minus_minus:
			return OperatorKind::Decrement;
		case std::meta::operators::op_comma:
			return OperatorKind::Comma;
		}

		return OperatorKind::Unknown;
	}

	template <std::meta::info MetaType, std::meta::info MetaOperator>
	consteval OperatorInfo MakeOperator(const Invoker invoke)
	{
		return OperatorInfo(MapOperatorKind(std::meta::operator_of(MetaOperator)),
							TypeReferenceTo<std::meta::remove_cvref(std::meta::return_type_of(MetaOperator))>(), IdentifierOf(MetaOperator),
							DisplayStringOf(MetaOperator), ScopePathOf<MetaOperator>(), MakeParameters<MetaOperator>(),
							MakeFunctionTraits<MetaOperator>(), invoke, MakeAnnotations<MetaOperator>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval std::array<OperatorInfo, sizeof...(I)> MakeOperatorArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto operators = std::define_static_array(GetOperatorFunctions(MetaType));

		// Invokers left null, set in place on demand (FillOperatorInvokers), so the metadata build never splices.
		return std::array<OperatorInfo, sizeof...(I)>{MakeOperator<MetaType, operators[I]>(nullptr)...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeOperatorsFromType()
	{
		if constexpr (ProvidesSupersedingFacet<MetaType>())
		{
			return std::array<OperatorInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto operatorCount = std::define_static_array(GetOperatorFunctions(MetaType)).size();
			return MakeOperatorArray<MetaType>(std::make_index_sequence<operatorCount>{});
		}
		else
		{
			return std::array<OperatorInfo, 0>{};
		}
	}

	// One mutable array per type; TypeInfo's GetOperators span points here, and the demand upgrade sets each
	// invoker in place. An OperatorInfo is a FunctionInfo, so SetInvoker fills its inherited invoker.
	template <std::meta::info MetaType>
	inline constinit auto GOperators = MakeOperatorsFromType<MetaType>();

	template <std::meta::info MetaType, std::size_t... I>
	void FillOperatorInvokersImpl(std::index_sequence<I...>)
	{
		using T = [:MetaType:];
		[[maybe_unused]] constexpr auto operators = std::define_static_array(GetOperatorFunctions(MetaType));
		((SetInvoker(GOperators<MetaType>[I], MakeInvoker<T, operators[I]>())), ...);
	}

	export template <std::meta::info MetaType>
	void FillOperatorInvokers()
	{
		constexpr auto count = std::define_static_array(GetOperatorFunctions(MetaType)).size();
		FillOperatorInvokersImpl<MetaType>(std::make_index_sequence<count>{});
	}
}
