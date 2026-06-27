module PlaygroundEngine.World;

PgE::GameObject* PgE::World::AddGameObject()
{
	_gameObjects.push_back(std::make_unique<GameObject>());

	return _gameObjects.back().get();
}

void PgE::World::Run()
{
	for (auto& gameObject : _gameObjects)
	{
		gameObject->Update();
	}
}
