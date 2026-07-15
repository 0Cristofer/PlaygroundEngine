export module PlaygroundEngine.Reflection.Core:ConstructorInfo;

import :TypedRef;
import :DeclarationInfo;
import :ParameterInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeOf();

	export enum class ConstructorKind : std::uint8_t
	{
		Default,
		Copy,
		Move,
		Converting,
		Other,
	};

	export struct ConstructError
	{
		enum Kind : std::uint8_t
		{
			ArityMismatch,
			TypeMismatch,
			NullArgument,
			ConstViolation,
			NotMovable,
			SlotTypeMismatch,
			NotConstructible,
			NoMatchingConstructor,
			AmbiguousConstructor,
		};

		Kind Reason;
		std::uint16_t ArgumentIndex = 0;
	};

	// A constructor builds an object into a caller-owned slot from erased arguments. It has no object
	// pointer and its "return" is the mandatory construction into the slot, so it is a distinct callable
	// from FunctionInfo, sharing only the argument binder. See docs/ReflectionInternals.md (lifetime ops).
	export using Constructor = std::expected<void, ConstructError> (*)(std::span<const TypedRef> args, const TypedRef& slot);

	export class ConstructorInfo : public DeclarationInfo
	{
	public:
		constexpr ConstructorInfo(const std::span<const ParameterInfo> params,
								  const ConstructorKind kind,
								  const bool isExplicit,
								  const std::string_view displayName,
								  const std::span<const std::string_view> scopePath,
								  const AccessKind access,
								  const Constructor construct,
								  const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo({}, displayName, scopePath, annotations), _params(params), _kind(kind), _isExplicit(isExplicit), _access(access),
			  _construct(construct)
		{}

		std::span<const ParameterInfo> GetParams() const
		{
			return _params;
		}
		ConstructorKind GetKind() const
		{
			return _kind;
		}
		bool IsExplicit() const
		{
			return _isExplicit;
		}
		AccessKind GetAccess() const
		{
			return _access;
		}

		// A constructor whose erased path is unusable (deleted, inaccessible, or unbindable) reflects as
		// metadata with no thunk, the way an rvalue-qualified function reflects with no invoker.
		bool CanConstruct() const
		{
			return _construct != nullptr;
		}

		// Whether these erased arguments name this constructor's signature. The binder admits no
		// conversions, so an exact tag match per parameter is the whole of it; a parameter's reference and
		// const qualifiers are not tags, which is why a copy/move pair matches the same arguments.
		bool MatchesArguments(const std::span<const TypedRef> args) const
		{
			if (args.size() != _params.size())
			{
				return false;
			}

			for (std::size_t index = 0; index < _params.size(); ++index)
			{
				if (&_params[index].GetTypeInfo() != args[index].Type)
				{
					return false;
				}
			}

			return true;
		}

		std::expected<void, ConstructError> Construct(const std::span<const TypedRef> args, const TypedRef& slot) const
		{
			if (!_construct)
			{
				return std::unexpected(ConstructError{.Reason = ConstructError::NotConstructible, .ArgumentIndex = 0});
			}

			return _construct(args, slot);
		}

		template <typename T, typename... Arguments>
		std::expected<T, ConstructError> ConstructAs(Arguments&&... arguments) const
		{
			return detail::ValueFromSlot<T, ConstructError>(
				[this](const std::span<const TypedRef> args, const TypedRef& slot) { return Construct(args, slot); },
				detail::MakeTypedRefs(std::forward<Arguments>(arguments)...));
		}

	private:
		std::span<const ParameterInfo> _params;
		ConstructorKind _kind = ConstructorKind::Other;
		bool _isExplicit = false;
		AccessKind _access = AccessKind::Public;
		Constructor _construct = nullptr;
	};
}
