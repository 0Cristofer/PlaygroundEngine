export module PlaygroundEngine.Reflection.TypeInfo:FuncInfo;

import std;

namespace PlaygroundEngine
{
	export class TypeInfo;

	export class ParamInfo
	{
	public:
		constexpr ParamInfo(const TypeInfo* typeInfo, const std::string_view name) :
			_typeInfo(typeInfo), _name(name)
		{
		}

		std::string_view GetName() const;
		const TypeInfo& GetTypeInfo() const;

	private:
		const TypeInfo* _typeInfo;
		std::string_view _name;
	};

	export class FuncInfo
	{
	public:
		constexpr FuncInfo(const TypeInfo* returnType, const std::string_view name,
		                   const std::span<const ParamInfo> params) :
			_returnType(returnType), _name(name), _params(params)
		{
		}

		std::string_view GetName() const;
		const TypeInfo& GetReturnType() const;
		std::span<const ParamInfo> GetParams() const;

	private:
		const TypeInfo* _returnType = nullptr;
		std::string_view _name;
		std::span<const ParamInfo> _params;
	};
}
