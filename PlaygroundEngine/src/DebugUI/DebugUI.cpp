module;

#include "PlaygroundEngine/Log.h"

#include "ImGuiContextDeclaration.h"

module PlaygroundEngine.DebugUi;

import imgui;
import imgui_internal;
import std;

import PlaygroundEngine.Files;
import PlaygroundEngine.Log;
import PlaygroundEngine.Paths;
import PlaygroundEngine.PlatformEvents;
import PlaygroundEngine.Reflection;
import PlaygroundEngine.WindowServer;

namespace PgE
{

#if defined(PGE_DEV)
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

	// The debug UI's own values ride in ImGui's settings file rather than a second one, through the
	// handler mechanism ImGui offers for exactly this. Window layout and this share one file.

	constexpr auto SettingsTypeName = "PlaygroundDebugUi";
	constexpr std::string_view FontScaleKey = "FontScale=";

	// File-local rather than a member for the same reason ImGui's own context is global: the code
	// that needs to ask is scattered through whatever the frame reaches, and holds no DebugUi.
	bool FrameOpen = false;

	namespace
	{
		// ImGui hands the borrowed server back through its own user-data slot, so nothing about the
		// connection has to be stored here. Its default handlers on this platform are a buffer local
		// to the context, which copies and pastes within the app and never reaches other programs.

		void SetClipboardText(ImGuiContext*, const char* text)
		{
			auto* windowServer = static_cast<WindowServer*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);

			windowServer->SetClipboardText(text != nullptr ? std::string_view(text) : std::string_view());
		}

		const char* GetClipboardText(ImGuiContext*)
		{
			const auto* windowServer = static_cast<WindowServer*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);

			// Parked in a buffer that outlives the call, because ImGui reads the returned pointer
			// after this returns and only drops it at the next request.

			static std::string clipboardText;

			std::optional<std::string> text = windowServer->GetClipboardText();
			if (!text)
			{
				return nullptr;
			}

			clipboardText = std::move(*text);

			return clipboardText.c_str();
		}
	}

	namespace
	{
		void* ReadSettingsOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
		{
			// One entry with a fixed name, so the token handed back to the line reader carries nothing.

			return std::string_view(name) == "Settings" ? const_cast<char*>(SettingsTypeName) : nullptr;
		}

		void ReadSettingsLine(ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line)
		{
			const std::string_view text(line);
			if (!text.starts_with(FontScaleKey))
			{
				return;
			}

			const std::string_view value = text.substr(FontScaleKey.size());

			if (float fontScale = 0.0f; std::from_chars(value.data(), value.data() + value.size(), fontScale).ec == std::errc{} && fontScale > 0.0f)
			{
				ImGui::GetStyle().FontScaleMain = fontScale;
			}
		}

		// ReSharper disable once CppParameterMayBeConstPtrOrRef
		void WriteSettings(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buffer)
		{
			buffer->appendf("[%s][Settings]\n", handler->TypeName);
			buffer->appendf("FontScale=%.2f\n\n", static_cast<double>(ImGui::GetStyle().FontScaleMain));
		}

		void RegisterSettingsHandler()
		{
			ImGuiSettingsHandler handler;
			handler.TypeName = SettingsTypeName;
			handler.TypeHash = ImHashStr(SettingsTypeName);
			handler.ReadOpenFn = &ReadSettingsOpen;
			handler.ReadLineFn = &ReadSettingsLine;
			handler.WriteAllFn = &WriteSettings;

			ImGui::AddSettingsHandler(&handler);
		}
	}

	DebugUi::DebugUi(const Window& window, WindowServer& windowServer)
	{
		ImGui::CreateContext();

		ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
		platformIo.Platform_SetClipboardTextFn = &SetClipboardText;
		platformIo.Platform_GetClipboardTextFn = &GetClipboardText;
		platformIo.Platform_ClipboardUserData = &windowServer;

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

		// Before the settings are read, since a handler registered afterward never sees its section.

		RegisterSettingsHandler();

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

		PGE_LOG(Info, "Debug UI scale: display {:.2f}, interface {:.2f}", displayScale, style.FontScaleMain);
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
	CursorShape DebugUi::DesiredCursor()
	{
		if (ImGui::GetCurrentContext() == nullptr)
		{
			return CursorShape::Arrow;
		}

		switch (ImGui::GetMouseCursor())
		{
		case ImGuiMouseCursor_TextInput:
			return CursorShape::TextInput;
		case ImGuiMouseCursor_Hand:
			return CursorShape::Hand;
		case ImGuiMouseCursor_NotAllowed:
			return CursorShape::NotAllowed;
		case ImGuiMouseCursor_ResizeEW:
			return CursorShape::ResizeHorizontal;
		case ImGuiMouseCursor_ResizeNS:
			return CursorShape::ResizeVertical;
		case ImGuiMouseCursor_ResizeNWSE:
			return CursorShape::ResizeTopLeftBottomRight;
		case ImGuiMouseCursor_ResizeNESW:
			return CursorShape::ResizeTopRightBottomLeft;
		case ImGuiMouseCursor_ResizeAll:
			return CursorShape::ResizeAll;
		case ImGuiMouseCursor_None:
			return CursorShape::Hidden;
		default:
			return CursorShape::Arrow;
		}
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
			if (ImGui::DragFloat("Font scale", &style.FontScaleMain, FontScaleDragStep, MinimumFontScale, MaximumFontScale, "%.2f"))
			{
				// ImGui tracks its own state for this, and knows nothing about ours changing.

				ImGui::MarkIniSettingsDirty();
			}
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
#else
	// Compiled out of shipping builds, which is what keeps ImGui's code from being linked at all:
	// nothing here references it, so the linker never pulls the objects that hold it.

	DebugUi::DebugUi(const Window&, WindowServer&)
	{}

	DebugUi::~DebugUi() = default;

	bool DebugUi::IsFrameOpen()
	{
		return false;
	}

	CursorShape DebugUi::DesiredCursor()
	{
		return CursorShape::Arrow;
	}

	void DebugUi::BeginFrame(const Window&, const PlatformEventRecord&, float) const
	{}

	void DebugUi::DrawSettingsPanel() const
	{}

	ImDrawData* DebugUi::EndFrame() const
	{
		return nullptr;
	}

	void DebugUi::LoadSettings() const
	{}

	void DebugUi::SaveSettings() const
	{}
#endif
}
