export module PlaygroundEngine.Reflection.TypeInfo;

export import :FieldInfo;
export import :FuncInfo;

import std;

namespace PgE
{
	export class TypeInfo
	{
	public:
		constexpr TypeInfo(const std::string_view name, const std::span<const FieldInfo> fields,
		                   const std::span<const FuncInfo> functions,
		                   std::string (*stringifyThunk)(const void*)) : _displayName(name),
		                                                                 _fields(fields),
		                                                                 _functions(functions),
		                                                                 _stringifyThunk(stringifyThunk)
		{
		}

		std::string_view GetDisplayName() const
		{
			return _displayName;
		}

		std::string ObjectToString(const void* obj) const;
		std::string FunctionsToString() const;

	private:
		std::string_view _displayName;
		std::span<const FieldInfo> _fields;
		std::span<const FuncInfo> _functions;
		std::string (*_stringifyThunk)(const void*) = nullptr;
	};
}
