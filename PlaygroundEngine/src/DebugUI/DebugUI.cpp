module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine.DebugUi;

import imgui;
import std;

import PlaygroundEngine.Files;
import PlaygroundEngine.Log;
import PlaygroundEngine.Paths;
import PlaygroundEngine.PlatformEvents;
import PlaygroundEngine.Reflection;
import PlaygroundEngine.WindowServer;

import :InputTranslation;

namespace PgE
{
	// ImGui aborts on a non-positive delta on any frame past the first, so a frame the clock reports
	// as instant is floored rather than passed through.
	constexpr float MinimumDeltaTimeSeconds = 1.0f / 10000.0f;

	// Matching ImGui's own style editor, so the range a reader can drag to is the one upstream
	// considers sane.

	constexpr float MinimumFontScale = 0.5f;
	constexpr float MaximumFontScale = 4.0f;
	constexpr float FontScaleDragStep = 0.02f;

	// Beside the executable, the way the renderer's shaders and textures are staged, so a clean build
	// starts from a clean layout.

	constexpr std::string_view SettingsFileName = "DebugUi.ini";

	// File-local rather than a member for the same reason ImGui's own context is global: the code
	// that needs to ask is scattered through whatever the frame reaches, and holds no DebugUi.
	bool FrameOpen = false;

	DebugUi::DebugUi(const Window& window, const float interfaceScale)
	{
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();

		// The scalable default rather than AddFontDefault's pick between it and the 13 pixel bitmap,
		// which is chosen from the size expected when the font is added and so would go stale the
		// moment the scale is dragged. Same face, drawn from outlines.

		io.Fonts->AddFontDefaultVector();

		// Both axes collapse into one factor because every window system this targets scales
		// uniformly. Two factors rather than one because ImGui separates what the display reports
		// from what the reader asked for, and only the second is worth a control.

		const ContentScale contentScale = window.GetContentScale();
		const float displayScale = std::max(contentScale.X, contentScale.Y);

		ImGuiStyle& style = ImGui::GetStyle();
		style.FontScaleDpi = displayScale;
		style.FontScaleMain = interfaceScale;

		PGE_LOG(Info, "Debug UI scale: display {:.2f}, interface {:.2f}", displayScale, interfaceScale);

		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// Null disables ImGui's own file handling, whose default writes an imgui.ini relative to the
		// working directory. The same settings are loaded and stored below through Files/Paths, which
		// keeps one I/O path and one file-error type, the way stb's is routed.

		io.IniFilename = nullptr;

		if (const std::expected<std::filesystem::path, PathError> directory = GetExecutableDirectory(); directory)
		{
			_settingsPath = *directory / SettingsFileName;
			LoadSettings();
		}
		else
		{
			PGE_LOG(Warn, "Debug UI settings disabled: {}", ToString(directory.error()));
		}

		PGE_LOG(Info, "Debug UI context created");
	}

	DebugUi::~DebugUi()
	{
		FrameOpen = false;

		// ImGui only flushes settings itself when it owns the file, so a change made inside the last
		// save interval would be lost on exit. The frame count guards against storing the empty
		// layout of a context that never ran a frame.

		if (ImGui::GetFrameCount() > 0)
		{
			SaveSettings();
		}

		ImGui::DestroyContext();
	}

	bool DebugUi::IsFrameOpen()
	{
		return FrameOpen;
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	void DebugUi::BeginFrame(const Window& window, const PlatformEventRecord& platformEventRecord, const float deltaTimeSeconds) const
	{
		ImGuiIO& io = ImGui::GetIO();

		// Screen coordinates rather than pixels, because that is the space pointer events arrive in.
		// The scale carries the difference, and is what the overlay pass multiplies by to reach the
		// framebuffer. A minimized window reports zero, which would make the scale a division by it.

		const WindowSize windowSize = window.GetSize();
		const FramebufferSize framebufferSize = window.GetFramebufferSize();

		io.DisplaySize = ImVec2(static_cast<float>(windowSize.Width), static_cast<float>(windowSize.Height));

		if (windowSize.Width > 0 && windowSize.Height > 0)
		{
			io.DisplayFramebufferScale = ImVec2(static_cast<float>(framebufferSize.Width) / static_cast<float>(windowSize.Width),
												static_cast<float>(framebufferSize.Height) / static_cast<float>(windowSize.Height));
		}

		io.DeltaTime = std::max(deltaTimeSeconds, MinimumDeltaTimeSeconds);

		SubmitPlatformEvents(platformEventRecord);

		ImGui::NewFrame();

		// A dockspace over the whole viewport, so panels dock to the window edges. The central node
		// is passthrough: it hosts no window of its own, leaving the scene visible under it.

		ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

		FrameOpen = true;
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	void DebugUi::DrawSettingsPanel() const
	{
		if (!FrameOpen)
		{
			return;
		}

		ImGuiStyle& style = ImGui::GetStyle();

		// Begin's result says whether the window is collapsed, not whether it opened, so End is
		// unconditional.

		// Auto-resizing because its own contents change size: dragging the scale is what resizes the
		// text inside it. The field is sized in font heights for the same reason, and narrowing it is
		// what leaves the auto-fit room for the label.

		if (ImGui::Begin("Debug UI", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
			ImGui::DragFloat("Font scale", &style.FontScaleMain, FontScaleDragStep, MinimumFontScale, MaximumFontScale, "%.2f");
			ImGui::Text("Display scale: %.2f", style.FontScaleDpi);
		}

		ImGui::End();
	}

	ImDrawData* DebugUi::EndFrame() const
	{
		// Tolerant rather than paired-or-else, matching how a failed overlay backend is treated: a
		// debug aid with no frame to close yields no draw data instead of taking the frame down.

		if (!FrameOpen)
		{
			return nullptr;
		}

		FrameOpen = false;

		// ImGui asks rather than writes, since its own file handling is off. The flag is ours to
		// clear once the data is stored.

		if (ImGuiIO& io = ImGui::GetIO(); io.WantSaveIniSettings)
		{
			SaveSettings();
			io.WantSaveIniSettings = false;
		}

		ImGui::Render();

		return ImGui::GetDrawData();
	}

	void DebugUi::LoadSettings() const
	{
		// A missing file is the first run, not a failure.

		if (const std::expected<std::string, FileError> settings = ReadTextFile(_settingsPath); settings)
		{
			ImGui::LoadIniSettingsFromMemory(settings->c_str(), settings->size());
		}
	}

	void DebugUi::SaveSettings() const
	{
		if (_settingsPath.empty())
		{
			return;
		}

		std::size_t size = 0;
		const char* settings = ImGui::SaveIniSettingsToMemory(&size);

		if (const std::expected<void, FileError> written = WriteBinaryFile(_settingsPath, std::as_bytes(std::span(settings, size))); !written)
		{
			PGE_LOG(Warn, "Debug UI settings not saved: {}", ToString(written.error()));
		}
	}
}
