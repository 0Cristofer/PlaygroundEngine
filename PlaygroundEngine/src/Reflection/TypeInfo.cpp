module PlaygroundEngine.Reflection;

import :FieldInfo;
import :FuncInfo;

import std;

namespace PgE
{
	std::string TypeInfo::ObjectToString(const void* obj) const
	{
		if (_stringifyThunk)
			return _stringifyThunk(obj);

		std::string out = "{";
		bool firstField = true;
		for (const FieldInfo& f : _fields)
		{
			const void* addr = static_cast<const std::byte*>(obj) + f.GetByteOffset();
			if (!firstField)
				out += ", ";
			firstField = false;
			out += f.GetName();
			out += ": ";
			out += f.GetTypeInfo().ObjectToString(addr);
		}
		return out + "}";
	}

	std::string TypeInfo::FunctionsToString() const
	{
		std::string out;
		bool firstFunc = true;
		for (const FuncInfo& function : _functions)
		{
			if (!firstFunc)
				out += '\n';
			firstFunc = false;
			out += function.GetReturnType().GetDisplayName();
			out += " ";
			out += function.GetName();
			out += "(";

			bool firstParam = true;
			for (const ParamInfo& param : function.GetParams())
			{
				if (!firstParam)
					out += ", ";
				firstParam = false;

				out += param.GetTypeInfo().GetDisplayName();
				if (!param.GetName().empty())
				{
					out += " ";
					out += param.GetName();
				}
			}

			out += ")";
		}
		return out;
	}

	std::span<const FuncInfo> TypeInfo::GetFunctions() const
	{
		return _functions;
	}

	std::vector<const FuncInfo*> TypeInfo::FindFunctionsByName(std::string_view name) const
	{
		// Linear scan: function counts per type are small and lookups happen at boundaries, not the
		// frame loop. Acceleration, if ever needed, belongs at the registry keyed by stable id.
		std::vector<const FuncInfo*> matches;
		for (const FuncInfo& function : _functions)
		{
			if (function.GetName() == name)
				matches.push_back(&function);
		}

		return matches;
	}
}
