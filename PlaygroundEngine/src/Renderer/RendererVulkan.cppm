export module PlaygroundEngine.Renderer.Vulkan;

import PlaygroundEngine.Window;

export import :VulkanTypes;

// An interface partition has to be reachable from the primary interface, and GCC rejects a plain
// 'import' of one here. Re-exporting publishes nothing, since :VulkanUtils exports no declaration.

export import :VulkanUtils;

import vulkan;
import std;

namespace PgE
{
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
					   vk::raii::DescriptorSetLayout descriptorSetLayout,
					   vk::raii::PipelineLayout pipelineLayout,
					   vk::raii::Pipeline graphicsPipeline,
					   vk::raii::CommandPool commandPool,
					   BufferResource vertexBufferResource,
					   BufferResource indexBufferResource,
					   std::vector<UniformBufferResource> uniformBufferResources,
					   vk::raii::DescriptorPool descriptorPool,
					   std::vector<vk::raii::DescriptorSet> descriptorSets,
					   std::vector<vk::raii::CommandBuffer> commandBuffer,
					   std::vector<vk::raii::Semaphore> presentCompleteSemaphores,
					   std::vector<vk::raii::Semaphore> renderFinishedSemaphores,
					   std::vector<vk::raii::Fence> inFlightFences)
			: _context(std::move(context)), _instance(std::move(instance)), _debugMessenger(std::move(debugMessenger)), _surface(std::move(surface)),
			  _physicalDevice(std::move(physicalDevice)), _logicalDevice(std::move(logicalDevice)), _queue(std::move(queue)),
			  _swapChain(std::move(swapChain)), _swapChainImages(std::move(swapChainImages)),
			  _swapChainSurfaceFormat(std::move(swapChainSurfaceFormat)), _swapChainExtent(std::move(swapChainExtent)),
			  _swapChainImageViews(std::move(swapChainImageViews)), _descriptorSetLayout(std::move(descriptorSetLayout)),
			  _pipelineLayout(std::move(pipelineLayout)), _graphicsPipeline(std::move(graphicsPipeline)), _commandPool(std::move(commandPool)),
			  _vertexBufferResource(std::move(vertexBufferResource)), _indexBufferResource(std::move(indexBufferResource)),
			  _uniformBufferResources(std::move(uniformBufferResources)), _descriptorPool(std::move(descriptorPool)),
			  _descriptorSets(std::move(descriptorSets)), _commandBuffers(std::move(commandBuffer)),
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
		void UpdateUniformBuffer(std::uint32_t frameIndex) const;

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
		vk::raii::DescriptorSetLayout _descriptorSetLayout;
		vk::raii::PipelineLayout _pipelineLayout;
		vk::raii::Pipeline _graphicsPipeline;
		vk::raii::CommandPool _commandPool;
		BufferResource _vertexBufferResource;
		BufferResource _indexBufferResource;
		std::vector<UniformBufferResource> _uniformBufferResources;
		vk::raii::DescriptorPool _descriptorPool;
		std::vector<vk::raii::DescriptorSet> _descriptorSets;
		std::vector<vk::raii::CommandBuffer> _commandBuffers;
		std::vector<vk::raii::Semaphore> _presentCompleteSemaphores;
		std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;
		std::vector<vk::raii::Fence> _inFlightFences;

		std::uint32_t _frameIndex = 0;
		bool _framebufferResized = false;
	};
}
