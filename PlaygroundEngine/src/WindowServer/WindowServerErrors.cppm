export module PlaygroundEngine.WindowServer:WindowServerErrors;

namespace PgE
{
	export enum class WindowServerError
	{
		ConnectionFailed,
	};

	export enum class WindowError
	{
		WindowCreationFailed,
	};

	export enum class VulkanWindowError
	{
		ExtensionsUnavailable,
		SurfaceCreationFailed
	};
}
