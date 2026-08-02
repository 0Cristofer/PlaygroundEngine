export module PlaygroundEngine.Renderer.Vulkan:VulkanTypes;

import std;

namespace PgE
{
	export struct RendererSpecification
	{
		std::string ApplicationName = "Playground";
		std::string EngineName = "No Engine";
	};

	export enum class RendererCreationErrorKind
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
		SurfaceCapabilitiesFormatsError,
		SwapChainCreationError,
		SwapChainImagesCreationError,
		ImageViewCreationError,
		ShaderLoadError,
		ShaderModuleCreateError,
		PipelineLayoutCreateError,
		DescriptorSetLayoutCreateError,
		PipelineCreateError,
		BufferCreateError,
		DeviceMemoryAllocationError,
		DeviceMemoryBindError,
		DeviceMemoryMapError,
		CantFindMemoryType,
		ImageCreateError,
		NoSupportedDepthFormat,
		TextureLoadError,
		LinearBlitUnsupported,
		ModelLoadError,
		SamplerCreateError,
		CommandPoolCreateError,
		CommandBufferCreateError,
		UniformBufferCreateError,
		DescriptorPoolCreateError,
		DescriptorSetCreateError,
		CommandBufferCopyError,
		SemaphoreCreateError,
		FenceCreateError,
	};

	export enum class RendererRenderErrorKind
	{
		CommandBufferBeginError,
		CommandBufferEndError,
		FenceWaitError,
		FenceResetError,
		UnableToAcquireSwapChainImage,
		UnableToResetCommandBuffer,
		QueueSubmitError,
		QueuePresentError,
		DeviceWaitIdleError,
		SwapChainRecreationError,
	};

	export template <typename T>
	class RendererError
	{
	public:
		explicit RendererError(const T kind, const std::string& message = "") : _kind(kind), _message(message)
		{}

		[[nodiscard]] T Kind() const
		{
			return _kind;
		}

		[[nodiscard]] std::string_view Message() const
		{
			return _message;
		}

	private:
		T _kind;
		std::string _message;
	};

	export template <typename T>
	using CreationResult = std::expected<T, RendererError<RendererCreationErrorKind>>;
}
