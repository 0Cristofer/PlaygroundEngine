module PlaygroundEngine.Reflection.TypeInfo;

import :FuncInfo;

namespace PgE
{
	std::string_view ParamInfo::GetName() const
	{
		return _name;
	}

	const TypeInfo& ParamInfo::GetTypeInfo() const
	{
		return *_typeInfo;
	}

	std::string_view FuncInfo::GetName() const
	{
		return _name;
	}

	const TypeInfo& FuncInfo::GetReturnType() const
	{
		return *_returnType;
	}

	std::span<const ParamInfo> FuncInfo::GetParams() const
	{
		return _params;
	}
}
