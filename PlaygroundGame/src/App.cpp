module;

#include <PlaygroundEngine/Log.h>

module PlaygroundGame;

import imgui;

import PlaygroundEngine.Components.TransformComponent;
import PlaygroundEngine.Log;
import PlaygroundEngine.DebugUi;
import PlaygroundEngine.Reflection.Core;
import PlaygroundEngine.Reflection.Builtins;

std::unique_ptr<PgE::AppBase> PgG::PlaygroundGameAppDescriptor::GetApp()
{
	return std::make_unique<App>();
}

void PgG::App::OnBooted(PgE::EngineContext&)
{
	PGE_LOG(Info);
}

void PgG::App::OnStartRun(PgE::World* world)
{
	static int a = 2;
	PgE::GameObject* gO1 = world->AddGameObject();
	PgE::TransformComponent* component = gO1->AddComponent<PgE::TransformComponent>();
	component->Position = a;
	PGE_LOG(Info, "{}", component->Position);
	a++;

	_camera.Type = Perspective;
	_camera.Enabled = false;
	_camera.Position = 2.5f;
	_camera.Id = 1;
	_camera.MainLens = Lens{.FocalLength = 35.0f, .Coating = Quality::Medium};
	_camera.Name = "Main";
	_camera.Exposures = {1.0f, 2.0f, 4.0f};

	// Points at itself, so the panel has a real cycle to walk: each expansion opens one more level.
	_camera.Parent = &_camera;
}

void PgG::App::OnStep()
{
	if (!PgE::DebugUi::IsFrameOpen())
	{
		return;
	}

	if (!ImGui::Begin("PlaygroundGame"))
	{
		ImGui::End();
		return;
	}

	PgE::DebugPanelDrawer::Draw(_camera);

	ImGui::End();

	if (!ImGui::Begin("PlaygroundGame values"))
	{
		ImGui::End();
		return;
	}

	const Camera c = _camera;
	PgE::DebugPanelDrawer::Draw(c);
	ImGui::Text("Position: %f", _camera.Position);
	ImGui::Text("Enabled: %s", PgE::ToString(_camera.Enabled).c_str());
	ImGui::Text("Type: %s", PgE::ToString(_camera.Type).c_str());
	ImGui::Text("RenderQuality: %s", PgE::ToString(_camera.RenderQuality).c_str());
	ImGui::Text("Bias: %s", PgE::ToString(_camera.Bias).c_str());
	ImGui::Text("Id: %d", _camera.Id);

	ImGui::End();
}
