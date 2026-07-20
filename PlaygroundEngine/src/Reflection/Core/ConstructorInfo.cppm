export module PlaygroundEngine.Reflection.Core:ConstructorInfo;

import :TypedRef;
import :DeclarationInfo;
import :ParameterInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeMetaOf();

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

	export struct ConstructorTraits
	{
		// The language facts of one constructor, the shape FunctionTraits uses. IsDeleted and IsConsteval
		// name the reasons a constructor reflects with no thunk that are language facts, so a null thunk is a
		// stated absence: with Access telling apart an inaccessible one, a residual null means "unbindable".
		AccessKind Access = AccessKind::Public;
		ConstructorKind Kind = ConstructorKind::Other;
		bool IsExplicit = false;

		bool IsDeleted = false;
		bool IsConsteval = false;

		// A compiler-generated constructor (an implicit or = default copy/move/default ctor), as opposed to a
		// user-written one. What tells a serializer the copy is memberwise, not custom.
		bool IsDefaulted = false;
	};

	export class ConstructorInfo : public DeclarationInfo
	{
	public:
		constexpr ConstructorInfo(const std::span<const ParameterInfo> params,
								  const std::string_view displayName,
								  const std::span<const std::string_view> scopePath,
								  const ConstructorTraits& traits,
								  const Constructor construct,
								  const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo({}, displayName, scopePath, annotations), _params(params), _traits(traits), _construct(construct)
		{}

		std::span<const ParameterInfo> GetParams() const
		{
			return _params;
		}
		const ConstructorTraits& GetTraits() const
		{
			return _traits;
		}
		ConstructorKind GetKind() const
		{
			return _traits.Kind;
		}
		bool IsExplicit() const
		{
			return _traits.IsExplicit;
		}
		AccessKind GetAccess() const
		{
			return _traits.Access;
		}
		bool IsDeleted() const
		{
			return _traits.IsDeleted;
		}
		bool IsConsteval() const
		{
			return _traits.IsConsteval;
		}
		bool IsDefaulted() const
		{
			return _traits.IsDefaulted;
		}

		// A constructor whose erased path is unusable (deleted, inaccessible, consteval, or unbindable)
		// reflects as metadata with no thunk; the traits name which, the way FunctionTraits does for a
		// function with no invoker.
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
		ConstructorTraits _traits;

		// Null until the type is named through TypeOf<T> (the demand upgrade sets it in place), and null after
		// that for a constructor whose erased path is unusable. SetConstructor is the only writer.
		Constructor _construct = nullptr;

		// Sets the thunk in place during the demand upgrade. A hidden friend rather than a public setter, so the
		// thunk stays write-once-by-the-builder (reached only through ADL), never caller-mutable.
		friend void SetConstructor(ConstructorInfo& constructor, const Constructor construct)
		{
			constructor._construct = construct;
		}
	};
}
