export module PlaygroundEngine.Window;

export import :common;

import std;

namespace PgE
{
	template <typename Backend>
	concept WindowBackendInterface = requires(Backend backend, const Backend constBackend, const WindowSpecification& specification) {
		{ Backend::Create(specification) } -> std::same_as<std::expected<std::unique_ptr<Backend>, WindowError>>;
		{ backend.PollEvents() } -> std::same_as<void>;
		{ constBackend.SwapBuffers() } -> std::same_as<void>;
		{ constBackend.ShouldClose() } -> std::same_as<bool>;
	};

	export class Window
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<Window>, WindowError> Create(const WindowSpecification& specification);

		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void PollEvents();
		void SwapBuffers() const;

		[[nodiscard]] bool ShouldClose() const;

		[[nodiscard]] int GetWidth() const
		{
			return _specification.Width;
		}
		[[nodiscard]] int GetHeight() const
		{
			return _specification.Height;
		}
		[[nodiscard]] std::string_view GetTitle() const
		{
			return _specification.Title;
		}

	private:
		Window(std::unique_ptr<WindowBackend> backend, WindowSpecification specification);

		std::unique_ptr<WindowBackend> _backend;
		WindowSpecification _specification;
	};
}
