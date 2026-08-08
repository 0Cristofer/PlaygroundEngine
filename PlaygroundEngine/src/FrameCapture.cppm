export module PlaygroundEngine.FrameCapture;

import std;

import PlaygroundEngine.PlatformEvents;
import PlaygroundEngine.Renderer.Vulkan;

namespace PgE
{
	/// Turns the capture key into a named request on the renderer, and owns everything naming a capture
	/// needs: the trigger binding, the captures directory, and the file names.
	export class FrameCapture
	{
	public:
		/// Once per frame, before the frame is drawn, so the key pressed this frame is served by this
		/// frame. Reports its own failures: a capture that cannot be named is logged, never returned.
		/// Main thread only.
		void ServiceRequests(const PlatformEventRecord& events, RendererVulkan& renderer);

	private:
		std::optional<std::filesystem::path> GenerateCapturePath();

		// Separates two captures landing in the same millisecond. Plain rather than atomic: naming
		// happens only here, on the frame loop's thread.

		std::uint64_t _nextCaptureIndex = 0;
	};
}
