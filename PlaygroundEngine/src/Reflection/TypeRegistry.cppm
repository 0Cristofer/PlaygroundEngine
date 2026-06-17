export module PlaygroundEngine.Reflection.TypeRegistry;

export import PlaygroundEngine.Reflection.TypeInfo;

import PlaygroundEngine.Reflection;

import std;

namespace PlaygroundEngine
{
	export class TypeRegistry
	{
	public:
		template <typename T>
		void RegisterType()
		{
			_registeredTypes.push_back(&TypeOf<T>());
		}

	private:
		std::vector<const TypeInfo*> _registeredTypes;
	};
}
