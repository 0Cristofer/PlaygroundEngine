module PlaygroundGame.Ecs.EntityDebugPanelSystem;

import PlaygroundEngine.Reflection;
import std;

namespace PgG
{
	void EntityDebugPanelSystem::Step(float)
	{
		if (!PgE::DebugUi::IsFrameOpen())
		{
			return;
		}

		if (!ImGui::Begin("Entity debug"))
		{
			ImGui::End();
			return;
		}

		for (const PgE::Entity entity : GetEntities())
		{
			if (!ImGui::TreeNodeEx(std::format("Entity {}", entity.Id).c_str()))
			{
				continue;
			}

			PgE::DebugPanelDrawer::Draw(entity);

			// Every component through its borrow, so the panel names no component type and a new one shows
			// up here the moment it carries an annotated field.
			for (const PgE::TypedRef& component : GetEntityComponents(entity))
			{
				if (!ImGui::TreeNodeEx(std::format("{}", component.Type->GetIdentifier()).c_str()))
				{
					continue;
				}

				PgE::DebugPanelDrawer::Draw(component);

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		ImGui::End();
	}
}
