module;

#include <meta>

export module PlaygroundEngine.Reflection.Contracts;

import std;

namespace PgE
{
	// Compile-time contracts on a type's machine-facing shape. Each is a consteval predicate meant to
	// be asserted at the type's definition (static_assert), so a change that breaks the contract is a
	// build error, not a silent regression. They use std::meta directly and depend on no engine state.

	// Safe to copy byte-for-byte across the wire and to C#: trivially copyable and standard layout, so
	// its object representation is portable and it maps one-to-one onto a C# struct.
	export template <typename T>
	consteval bool IsTriviallyReplicable()
	{
		return std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;
	}

	// No padding holes (internal or tail) and no bitfields, so the object representation is fully
	// determined by the fields. Required wherever the raw bytes are hashed or compared for determinism.
	export template <typename T>
	consteval bool HasNoPadding()
	{
		if (std::is_empty_v<T>)
		{
			return true;
		}

		std::size_t expected = 0;
		for (const std::meta::info member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
		{
			const auto [bytes, bits] = std::meta::offset_of(member);
			if (bits != 0)
			{
				return false;
			}

			const std::size_t byteOffset = static_cast<std::size_t>(bytes);
			if (byteOffset != expected)
			{
				return false;
			}

			expected = byteOffset + std::meta::size_of(std::meta::type_of(member));
		}

		return expected == sizeof(T);
	}

	// Fits within a byte budget: a compile-time size guard for wire packets, ECS components, or handles.
	export template <typename T, std::size_t MaxBytes>
	consteval bool FitsBudget()
	{
		return sizeof(T) <= MaxBytes;
	}
}
