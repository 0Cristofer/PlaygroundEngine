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
		PipelineCreateError,
		CommandPoolCreateError,
		CommandBufferCreateError,
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

	export class RendererVulkan
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<RendererVulkan>, RendererError<RendererCreationErrorKind>> Create(
			const RendererSpecification& specification, const Window& window);

		void Teardown() const;

		std::expected<void, RendererError<RendererRenderErrorKind>> DrawFrame(FramebufferSize framebufferSize);

		void NotifyFramebufferResized();

	private:
		RendererVulkan(vk::raii::Context context,
					   vk::raii::Instance instance,
					   vk::raii::DebugUtilsMessengerEXT debugMessenger,
					   vk::raii::SurfaceKHR surface,
					   vk::raii::PhysicalDevice physicalDevice,
					   vk::raii::Device logicalDevice,
					   vk::raii::Queue queue,
					   vk::raii::SwapchainKHR swapChain,
					   std::vector<vk::Image> swapChainImages,
					   vk::SurfaceFormatKHR swapChainSurfaceFormat,
					   vk::Extent2D swapChainExtent,
					   std::vector<vk::raii::ImageView> swapChainImageViews,
					   vk::raii::PipelineLayout pipelineLayout,
					   vk::raii::Pipeline graphicsPipeline,
					   vk::raii::CommandPool commandPool,
					   std::vector<vk::raii::CommandBuffer> commandBuffer,
					   std::vector<vk::raii::Semaphore> presentCompleteSemaphores,
					   std::vector<vk::raii::Semaphore> renderFinishedSemaphores,
					   std::vector<vk::raii::Fence> inFlightFences)
			: _context(std::move(context)), _instance(std::move(instance)), _debugMessenger(std::move(debugMessenger)), _surface(std::move(surface)),
			  _physicalDevice(std::move(physicalDevice)), _logicalDevice(std::move(logicalDevice)), _queue(std::move(queue)),
			  _swapChain(std::move(swapChain)), _swapChainImages(std::move(swapChainImages)),
			  _swapChainSurfaceFormat(std::move(swapChainSurfaceFormat)), _swapChainExtent(std::move(swapChainExtent)),
			  _swapChainImageViews(std::move(swapChainImageViews)), _pipelineLayout(std::move(pipelineLayout)),
			  _graphicsPipeline(std::move(graphicsPipeline)), _commandPool(std::move(commandPool)), _commandBuffers(std::move(commandBuffer)),
			  _presentCompleteSemaphores(std::move(presentCompleteSemaphores)), _renderFinishedSemaphores(std::move(renderFinishedSemaphores)),
			  _inFlightFences(std::move(inFlightFences))
		{}

		void TransitionImageLayout(std::uint32_t imageIndex,
								   vk::ImageLayout oldLayout,
								   vk::ImageLayout newLayout,
								   vk::AccessFlags2 srcAccessMask,
								   vk::AccessFlags2 dstAccessMask,
								   vk::PipelineStageFlags2 srcStageMask,
								   vk::PipelineStageFlags2 dstStageMask) const;
		std::expected<void, RendererError<RendererRenderErrorKind>> RecordCommandBuffer(std::uint32_t imageIndex) const;
		std::expected<void, RendererError<RendererRenderErrorKind>> RecreateSwapChain(FramebufferSize framebufferSize);

		vk::raii::Context _context;
		vk::raii::Instance _instance;
		vk::raii::DebugUtilsMessengerEXT _debugMessenger;
		vk::raii::SurfaceKHR _surface;
		vk::raii::PhysicalDevice _physicalDevice;
		vk::raii::Device _logicalDevice;
		vk::raii::Queue _queue;
		vk::raii::SwapchainKHR _swapChain;
		std::vector<vk::Image> _swapChainImages;
		vk::SurfaceFormatKHR _swapChainSurfaceFormat;
		vk::Extent2D _swapChainExtent;
		std::vector<vk::raii::ImageView> _swapChainImageViews;
		vk::raii::PipelineLayout _pipelineLayout;
		vk::raii::Pipeline _graphicsPipeline;
		vk::raii::CommandPool _commandPool;
		std::vector<vk::raii::CommandBuffer> _commandBuffers;
		std::vector<vk::raii::Semaphore> _presentCompleteSemaphores;
		std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;
		std::vector<vk::raii::Fence> _inFlightFences;

		std::uint32_t _frameIndex = 0;
		bool _framebufferResized = false;
	};
}
