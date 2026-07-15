module PlaygroundEngine.Reflection.Core;

import :FieldInfo;
import :FunctionInfo;
import :ConstructorInfo;
import :TypedRef;
import :Facets;

import std;

namespace
{
	using namespace PgE;

	bool SuitsArgument(const ParameterInfo& param, const TypedRef& arg)
	{
		// A const argument is not offered up however the caller tagged it, so moving out of it is never
		// what was meant. Everything else the caller may take either way, and prefers to be handed over.
		const bool offersMove = arg.Movable && !arg.IsConst;
		return param.BindsByMove() == offersMove;
	}

	bool BindsBetterThan(const ConstructorInfo& left, const ConstructorInfo& right, const std::span<const TypedRef> args)
		pre(left.GetParams().size() == args.size()) pre(right.GetParams().size() == args.size())
	{
		// Overload resolution's rule, over the one axis the erased arguments carry: better in at least one
		// parameter and worse in none. The preconditions hold because ranking only ever compares candidates
		// that cleared MatchesArguments, which is also the only place a same-tag pair gets separated.
		bool betterSomewhere = false;
		for (std::size_t index = 0; index < args.size(); ++index)
		{
			const bool leftSuits = SuitsArgument(left.GetParams()[index], args[index]);
			const bool rightSuits = SuitsArgument(right.GetParams()[index], args[index]);

			if (leftSuits && !rightSuits)
			{
				betterSomewhere = true;
			}
			else if (rightSuits && !leftSuits)
			{
				return false;
			}
		}

		return betterSomewhere;
	}
}

namespace PgE
{
	std::span<const FunctionInfo> TypeInfo::GetFunctions() const
	{
		return _functions;
	}

	std::vector<const FunctionInfo*> TypeInfo::FindFunctionsByIdentifier(const std::string_view identifier) const
	{
		// Linear scan: function counts per type are small and lookups happen at boundaries, not the
		// frame loop. Acceleration, if ever needed, belongs at the registry keyed by stable id.
		std::vector<const FunctionInfo*> matches;
		for (const FunctionInfo& function : _functions)
		{
			if (function.GetIdentifier() == identifier)
			{
				matches.push_back(&function);
			}
		}

		return matches;
	}

	const ConstructorInfo* TypeInfo::FindConstructor(const ConstructorKind kind) const
	{
		// Default, copy, and move are unique per type, so the first match is the only one; Converting and
		// Other are not, and a caller that needs a specific one of those selects by arguments instead.
		for (const ConstructorInfo& constructor : _constructors)
		{
			if (constructor.GetKind() == kind)
			{
				return &constructor;
			}
		}

		return nullptr;
	}

	std::expected<const ConstructorInfo*, ConstructError> TypeInfo::SelectConstructor(const std::span<const TypedRef> args) const
	{
		// A constructor with no thunk (deleted, inaccessible, consteval) is not a candidate: selection must
		// never resolve to something the erased path cannot call.
		std::vector<const ConstructorInfo*> candidates;
		for (const ConstructorInfo& constructor : _constructors)
		{
			if (constructor.CanConstruct() && constructor.MatchesArguments(args))
			{
				candidates.push_back(&constructor);
			}
		}

		if (candidates.empty())
		{
			return std::unexpected(ConstructError{.Reason = ConstructError::NoMatchingConstructor, .ArgumentIndex = 0});
		}

		const ConstructorInfo* best = candidates.front();
		for (const ConstructorInfo* candidate : candidates)
		{
			if (BindsBetterThan(*candidate, *best, args))
			{
				best = candidate;
			}
		}

		// Being the best of the pairwise sweep is not the same as beating every other candidate, so confirm
		// it before committing: a tie means the arguments genuinely do not name one constructor.
		for (const ConstructorInfo* candidate : candidates)
		{
			if (candidate != best && !BindsBetterThan(*best, *candidate, args))
			{
				return std::unexpected(ConstructError{.Reason = ConstructError::AmbiguousConstructor, .ArgumentIndex = 0});
			}
		}

		return best;
	}

	const ConstructorInfo* TypeInfo::FindConstructor(const std::span<const TypedRef> args) const
	{
		const auto selected = SelectConstructor(args);
		return selected ? *selected : nullptr;
	}

	std::expected<void, ConstructError> TypeInfo::Construct(const std::span<const TypedRef> args, const TypedRef& slot) const
	{
		const auto selected = SelectConstructor(args);
		if (!selected)
		{
			return std::unexpected(selected.error());
		}

		return (*selected)->Construct(args, slot);
	}

	const FieldInfo* TypeInfo::FindFieldByIdentifier(const std::string_view identifier) const
	{
		// Fields are unique by identifier, so the first match is the only one. Linear scan, same rationale
		// as FindFunctionsByIdentifier.
		for (const FieldInfo& field : _fields)
		{
			if (field.GetIdentifier() == identifier)
			{
				return &field;
			}
		}

		return nullptr;
	}

	std::expected<void, FieldError> TypeInfo::GetFieldValue(const void* obj, const std::string_view identifier, const TypedRef& out) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
		{
			return std::unexpected(FieldError{FieldError::FieldNotFound});
		}

		return field->GetValue(obj, out);
	}

	std::expected<void, FieldError> TypeInfo::SetFieldValue(void* obj, const std::string_view identifier, const TypedRef& in) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
		{
			return std::unexpected(FieldError{FieldError::FieldNotFound});
		}

		return field->SetValue(obj, in);
	}

	std::expected<TypedRef, FieldError> TypeInfo::GetFieldRef(void* obj, const std::string_view identifier) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
		{
			return std::unexpected(FieldError{FieldError::FieldNotFound});
		}

		return field->GetRef(obj);
	}

	std::expected<TypedRef, FieldError> TypeInfo::GetFieldRef(const void* obj, const std::string_view identifier) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
		{
			return std::unexpected(FieldError{FieldError::FieldNotFound});
		}

		return field->GetRef(obj);
	}
}
