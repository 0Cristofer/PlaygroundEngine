module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine.DebugUi;

import imgui;
import std;

import PlaygroundEngine.Log;
import PlaygroundEngine.WindowServer;

namespace PgE
{
	// ImGui aborts on a non-positive delta on any frame past the first, so a frame the clock reports
	// as instant is floored rather than passed through.
	constexpr float MinimumDeltaTimeSeconds = 1.0f / 10000.0f;

	// File-local rather than a member for the same reason ImGui's own context is global: the code
	// that needs to ask is scattered through whatever the frame reaches, and holds no DebugUi.
	bool FrameOpen = false;

	DebugUi::DebugUi()
	{
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();

		// Layout persistence is off rather than left at ImGui's default, which would fopen an
		// imgui.ini beside the working directory. It comes back through Files/Paths, the way stb's
		// I/O does, once dock layouts are worth keeping.

		io.IniFilename = nullptr;

		PGE_LOG(Info, "Debug UI context created");
	}

	DebugUi::~DebugUi()
	{
		FrameOpen = false;

		ImGui::DestroyContext();
	}

	bool DebugUi::IsFrameOpen()
	{
		return FrameOpen;
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	void DebugUi::BeginFrame(const FramebufferSize framebufferSize, const float deltaTimeSeconds) const
	{
		ImGuiIO& io = ImGui::GetIO();

		// Sized in pixels, so the framebuffer rather than the window: the two differ on a scaled
		// display, and the overlay pass renders at framebuffer resolution.

		io.DisplaySize = ImVec2(static_cast<float>(framebufferSize.Width), static_cast<float>(framebufferSize.Height));
		io.DeltaTime = std::max(deltaTimeSeconds, MinimumDeltaTimeSeconds);

		ImGui::NewFrame();

		FrameOpen = true;
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	ImDrawData* DebugUi::EndFrame() const
	{
		// Tolerant rather than paired-or-else, matching how a failed overlay backend is treated: a
		// debug aid with no frame to close yields no draw data instead of taking the frame down.

		if (!FrameOpen)
		{
			return nullptr;
		}

		FrameOpen = false;

		ImGui::Render();

		return ImGui::GetDrawData();
	}
}
