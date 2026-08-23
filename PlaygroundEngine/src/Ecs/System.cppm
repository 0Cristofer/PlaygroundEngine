export module PlaygroundEngine.Ecs:System;

import PlaygroundEngine.Ecs.Component;
import PlaygroundEngine.Ecs.Entity;
import PlaygroundEngine.Reflection.Core;

import std;

namespace PgE
{
	export class Ecs;

	export class System
	{
	public:
		virtual void Step(float deltaTimeSeconds) = 0;

		virtual ~System() = default;

	protected:
		explicit System(Ecs& ecs);

		Entity AddEntity() const;

		[[nodiscard]] bool IsEntityAlive(Entity entity) const;
		[[nodiscard]] std::vector<Entity> GetEntities() const;

		template <typename TComponent>
		std::shared_ptr<TComponent> AddComponentToEntity(Entity entity);

		template <typename TComponent>
		[[nodiscard]] std::shared_ptr<TComponent> TryGetComponent(Entity entity) const;

		template <typename TComponent>
		[[nodiscard]] std::vector<std::shared_ptr<TComponent>> GetComponents() const;

		template <typename TComponent>
		[[nodiscard]] std::vector<std::pair<Entity, std::shared_ptr<TComponent>>> GetComponentsWithEntities() const;

		[[nodiscard]] std::vector<TypedRef> GetEntityComponents(Entity entity) const;

		void DestroyEntity(Entity entity) const;

		template <typename TComponent>
		void DestroyComponent(Entity entity) const;

	private:
		Ecs& _ecs;
	};
}
