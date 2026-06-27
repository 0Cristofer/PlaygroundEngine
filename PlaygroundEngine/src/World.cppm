export module PlaygroundEngine.World;

import PlaygroundEngine.GameObject;

import std;

namespace PgE
{
	export class World
	{
	public:
		GameObject* AddGameObject();
		void Run();

	private:
		std::vector<std::unique_ptr<GameObject>> _gameObjects;
	};
}
