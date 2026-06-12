module PlaygroundEngine.GameObject;

namespace PlaygroundEngine
{
	void GameObject::Update()
	{
		for (const auto& component : _components | std::views::values)
		{
			component->Update();
		}
	}

	GameObject::ComponentId GameObject::IncrementComponentId()
	{
		static ComponentId id = 0;
		return id++;
	}
}
