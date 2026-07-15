export module PlaygroundEngine.Reflection.Core:EntityInfo;

import std;

namespace PgE
{
	export enum class AccessKind : std::uint8_t
	{
		Public,
		Protected,
		Private,
	};

	export class EntityInfo
	{
		// What every named entity has: what it is called and where it lives. Entity is the language's own
		// term ([basic.pre]/3), and it is what admits a template alongside a type, a member, and a function.
		// Annotations are not here: only a declaration can carry them. See docs/ReflectionInternals.md.

	public:
		constexpr EntityInfo(const std::string_view identifier, const std::string_view displayName, const std::span<const std::string_view> scopePath)
			: _identifier(identifier), _displayName(displayName), _scopePath(scopePath)
		{}

		// The structural query key: what the author wrote where the language spells a name, and a name
		// derived from structure where it does not (a fundamental type is int32). Empty where neither exists.
		std::string_view GetIdentifier() const
		{
			return _identifier;
		}

		// Implementation-defined diagnostic text, always present, never a key.
		std::string_view GetDisplayName() const
		{
			return _displayName;
		}

		// The enclosing named scopes, outermost first (PgE::Game::Weapon is ["PgE", "Game"]): the chain, not
		// a rendered string, so the separator stays the consumer's choice. It is what distinguishes A::Foo
		// from B::Foo, and it crosses inline namespaces. See docs/ReflectionInternals.md (scope).
		std::span<const std::string_view> GetScopePath() const
		{
			return _scopePath;
		}

	private:
		std::string_view _identifier;
		std::string_view _displayName;
		std::span<const std::string_view> _scopePath;
	};
}
