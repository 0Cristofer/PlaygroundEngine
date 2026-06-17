module PlaygroundEngine.Reflection.TypeInfo;

import :FieldInfo;
import :FuncInfo;

import std;

namespace PlaygroundEngine
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
}
