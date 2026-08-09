module PlaygroundEngine.DebugUi:InputTranslation;

import PlaygroundEngine.PlatformEvents;

namespace PgE
{
	// Feeds one frame of platform events into ImGui's input queue. Declared here so the tables and
	// ImGui's enumerator list stay in the implementation unit, the way the GLFW translation does it.

	void SubmitPlatformEvents(const PlatformEventRecord& platformEventRecord);
}
