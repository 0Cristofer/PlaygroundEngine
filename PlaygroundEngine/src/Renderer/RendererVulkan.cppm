export module PlaygroundEngine.RendererVulkan;

import PlaygroundEngine.Window;

import vulkan;
import std;

namespace PgE
{
	export struct RendererSpecification
	{
		std::string ApplicationName = "Playground";
		std::string EngineName = "No Engine";
	};

	export enum class RendererErrorKind
	{
		UnableToRequestExtensions,
		ExtensionsEnumerationError,
		ValidationLayersEnumerationError,
		ExtensionUnavailable,
		ValidationLayerUnavailable,
		InstanceCreationError,
		DebugMessengerCreationError,
		PhysicalDevicesEnumerationError,
		NoPhysicalDevices,
		NoSuitablePhysicalDevices,
		LogicalDeviceCreationError,
		UnableToCreateWindowSurface,
		SuitableQueueNotFound,
	};

	export class RendererError
	{
	public:
		explicit RendererError(const RendererErrorKind kind, const std::string& message = "") : _kind(kind), _message(message)
		{}

		[[nodiscard]] RendererErrorKind Kind() const
		{
			return _kind;
		}

		[[nodiscard]] std::string_view Message() const
		{
			return _message;
		}

	private:
		RendererErrorKind _kind;
		std::string _message;
	};

	export class RendererVulkan
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<RendererVulkan>, RendererError> Create(const RendererSpecification& specification,
																								  const Window& window);

	private:
		RendererVulkan(vk::raii::Context context,
					   vk::raii::Instance instance,
					   vk::raii::DebugUtilsMessengerEXT debugMessenger,
					   vk::raii::SurfaceKHR surface,
					   vk::raii::PhysicalDevice physicalDevice,
					   vk::raii::Device logicalDevice,
					   vk::raii::Queue queue)
			: _context(std::move(context)), _instance(std::move(instance)), _debugMessenger(std::move(debugMessenger)), _surface(std::move(surface)),
			  _physicalDevice(std::move(physicalDevice)), _logicalDevice(std::move(logicalDevice)), _queue(std::move(queue))
		{}

		vk::raii::Context _context;
		vk::raii::Instance _instance;
		vk::raii::DebugUtilsMessengerEXT _debugMessenger;
		vk::raii::SurfaceKHR _surface;
		vk::raii::PhysicalDevice _physicalDevice;
		vk::raii::Device _logicalDevice;
		vk::raii::Queue _queue;
	};
}
