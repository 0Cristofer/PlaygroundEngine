export module PlaygroundEngine.DebugUi;

import imgui;

import PlaygroundEngine.PlatformEvents;
import PlaygroundEngine.WindowServer;

namespace PgE
{
	/// Owns the ImGui context and brackets the frame it is drawn in. There is no panel registration:
	/// ImGui's context is the registry, so any code reached between BeginFrame and EndFrame can call
	/// ImGui:: and its windows land in that frame.
	export class DebugUi
	{
	public:
		/// The window supplies the display's density hint and interfaceScale the reader's starting
		/// preference, which ImGui keeps as separate factors and multiplies. Only text is scaled;
		/// spacing and padding keep their pixel sizes.
		explicit DebugUi(const Window& window, float interfaceScale = 1.0f);
		~DebugUi();

		DebugUi(const DebugUi&) = delete;
		DebugUi& operator=(const DebugUi&) = delete;

		/// Whether ImGui calls are safe right now: a context is alive and its frame is open. Static
		/// because panel code drawing from inside a system has no DebugUi to ask, and without a
		/// context ImGui::Begin dereferences a null pointer before it asserts anything.
		[[nodiscard]] static bool IsFrameOpen();

		/// Opens the ImGui frame, feeding it this frame's input. Pairs with EndFrame.
		void BeginFrame(const Window& window, const PlatformEventRecord& platformEventRecord, float deltaTimeSeconds) const;

		void DrawSettingsPanel() const;

		/// Closes the ImGui frame and returns its draw data, valid until the next BeginFrame. Hand it
		/// to the renderer, which draws it in its overlay pass. Null when no frame was open.
		[[nodiscard]] ImDrawData* EndFrame() const;
	};
}
