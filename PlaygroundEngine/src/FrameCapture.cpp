module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine.FrameCapture;

import std;
import PlaygroundEngine.Log;
import PlaygroundEngine.Paths;

namespace PgE
{
	namespace
	{
		constexpr std::string_view CAPTURE_DIRECTORY_NAME = "captures";
		constexpr auto CAPTURE_KEY = InputCode::KeyF12;
	}

	void FrameCapture::ServiceRequests(const PlatformEventRecord& events, RendererVulkan& renderer)
	{
		// The one entry point: an agent asking for a screenshot injects this same key, so it exercises the
		// path a person at the keyboard takes rather than a parallel one that could rot unnoticed.

		const bool captureRequested = std::ranges::any_of(events.GetEvents(), [](const PlatformEvent& event) {
			return event.Type == PlatformEventType::KeyPressed && event.Code == CAPTURE_KEY && !event.Repeat;
		});

		if (!captureRequested)
		{
			return;
		}

		PGE_LOG(Trace, "Frame capture requested");

		if (const std::optional<std::filesystem::path> capturePath = GenerateCapturePath())
		{
			renderer.RequestCapture(*capturePath);
		}
		else
		{
			PGE_LOG(Error, "Frame capture skipped: no usable capture path");
		}
	}

	std::optional<std::filesystem::path> FrameCapture::GenerateCapturePath()
	{
		const std::expected<std::filesystem::path, PathError> executableDirectory = GetExecutableDirectory();
		if (!executableDirectory)
		{
			return std::nullopt;
		}

		const std::filesystem::path capturesDirectory = *executableDirectory / CAPTURE_DIRECTORY_NAME;

		// WriteBinaryFile does not create parents, so the directory has to exist before the capture
		// reaches it rather than after.

		std::error_code errorCode;
		std::filesystem::create_directories(capturesDirectory, errorCode);

		if (errorCode)
		{
			return std::nullopt;
		}

		// Names are unique across runs, not just within one

		const std::chrono::time_point capturedAt = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());

		return capturesDirectory / std::format("capture-{:%Y%m%d-%H%M%S}-{}.png", capturedAt, _nextCaptureIndex++);
	}
}
