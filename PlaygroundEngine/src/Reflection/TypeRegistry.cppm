module;

#include <meta>

export module PlaygroundEngine.Reflection.TypeRegistry;

import PlaygroundEngine.Reflection.TypeInfo;

import std;

namespace PlaygroundEngine
{
	export class TypeRegistry
	{
	public:
		template <typename T>
		void RegisterType()
		{
			_registeredTypes.push_back(&TypeInfo::TypeOf<T>());
		}

	private:
		std::vector<const TypeInfo*> _registeredTypes;
	};
}
