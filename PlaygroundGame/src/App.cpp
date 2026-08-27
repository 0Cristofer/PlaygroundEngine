module;

#include <PlaygroundEngine/Log.h>

module PlaygroundGame;

import imgui;

import PlaygroundEngine.Log;
import PlaygroundEngine.DebugUi;
import PlaygroundGame.Ecs.EntitySpawnerSystem;
import PlaygroundGame.Ecs.EntityDebugPanelSystem;
import PlaygroundGame.Ecs.CameraControlSystem;
import PlaygroundGame.Ecs.FreeLookComponent;
import PlaygroundGame.Ecs.MovementSystem;
import PlaygroundGame.Ecs.PlayerControlledComponent;
import PlaygroundGame.Ecs.EntityNameComponent;
import PlaygroundEngine.Ecs.CameraComponent;
import PlaygroundEngine.Math;

std::unique_ptr<PgE::AppBase> PgG::PlaygroundGameAppDescriptor::GetApp()
{
	return std::make_unique<App>();
}

void PgG::App::OnBooted(PgE::EngineContext&)
{
	PGE_LOG(Info);
}

void PgG::App::OnStartRun(PgE::Ecs& ecs)
{
	auto inputSystem = std::make_unique<PgE::InputSystem>(ecs);
	_inputSystem = inputSystem.get();
	ecs.AddSystem(std::move(inputSystem));

	auto entitySpawnerSystem = std::make_unique<EntitySpawnerSystem>(ecs);
	ecs.AddSystem(std::move(entitySpawnerSystem));

	auto movementSystem = std::make_unique<MovementSystem>(ecs);
	ecs.AddSystem(std::move(movementSystem));

	auto cameraControlSystem = std::make_unique<CameraControlSystem>(ecs);
	ecs.AddSystem(std::move(cameraControlSystem));

	auto entityDebugPanelSystem = std::make_unique<EntityDebugPanelSystem>(ecs);
	ecs.AddSystem(std::move(entityDebugPanelSystem));

	SpawnPlayer(ecs);
	SpawnCamera(ecs);
}

void PgG::App::SpawnPlayer(PgE::Ecs& ecs)
{
	const PgE::Entity player = ecs.AddEntity();

	const std::shared_ptr<EntityNameComponent> name = ecs.AddComponentToEntity<EntityNameComponent>(player);
	name->Name = "Player";

	ecs.AddComponentToEntity<PgE::TransformComponent>(player);
	ecs.AddComponentToEntity<PlayerControlledComponent>(player);
}

void PgG::App::SpawnCamera(PgE::Ecs& ecs)
{
	const PgE::Entity camera = ecs.AddEntity();

	const std::shared_ptr<EntityNameComponent> name = ecs.AddComponentToEntity<EntityNameComponent>(camera);
	name->Name = "Camera";

	// Placed and aimed at the origin, which is where the one model the renderer draws sits.

	const std::shared_ptr<PgE::TransformComponent> transform = ecs.AddComponentToEntity<PgE::TransformComponent>(camera);
	transform->Position = PgE::Vector3{.X = 2.0f, .Y = -2.0f, .Z = 2.0f};
	transform->Rotation = PgE::Quaternion::FromEulerAngles(PgE::EulerAngles{.Pitch = PgE::ToRadians(-35.264f), .Yaw = PgE::ToRadians(45.0f)});

	ecs.AddComponentToEntity<PgE::CameraComponent>(camera);
	ecs.AddComponentToEntity<FreeLookComponent>(camera);
}

void PgG::App::OnStep(const PgE::PlatformEventRecord& platformEventRecord)
{
	_inputSystem->UpdatePlatformInput(platformEventRecord);
}
