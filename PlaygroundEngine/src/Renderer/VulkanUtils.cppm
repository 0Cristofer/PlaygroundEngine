export module PlaygroundEngine.Renderer.Vulkan:VulkanUtils;

import std;
import vulkan;

import PlaygroundEngine.Window;

import :VulkanTypes;

namespace PgE
{
	struct SwapChainResources
	{
		vk::raii::SwapchainKHR SwapChain;
		std::vector<vk::Image> Images;
		std::vector<vk::raii::ImageView> ImageViews;
		vk::Extent2D Extent;
	};

	struct BufferResource
	{
		vk::raii::Buffer Buffer;
		vk::raii::DeviceMemory DeviceMemory;
	};

	CreationResult<vk::raii::Instance> CreateInstance(const vk::raii::Context& context,
													  const RendererSpecification& specification,
													  const Window& window);

	CreationResult<vk::raii::DebugUtilsMessengerEXT> CreateDebugMessenger(const vk::raii::Instance& instance);

	CreationResult<vk::raii::SurfaceKHR> CreateSurface(const vk::raii::Instance& instance, const Window& window);

	CreationResult<vk::raii::PhysicalDevice> SelectPhysicalDevice(const vk::raii::Instance& instance,
																  std::span<const char* const> requiredExtensions);

	CreationResult<std::uint32_t> FindGraphicsPresentQueueFamily(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface);

	CreationResult<vk::raii::Device> CreateLogicalDevice(const vk::raii::PhysicalDevice& physicalDevice,
														 std::uint32_t queueFamilyIndex,
														 std::span<const char* const> requiredExtensions);

	CreationResult<vk::SurfaceFormatKHR> SelectSurfaceFormat(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface);

	CreationResult<SwapChainResources> CreateSwapChainResources(const vk::raii::PhysicalDevice& physicalDevice,
																const vk::raii::Device& logicalDevice,
																const vk::raii::SurfaceKHR& surface,
																FramebufferSize framebufferSize,
																const vk::SurfaceFormatKHR& swapChainSurfaceFormat);

	CreationResult<vk::raii::PipelineLayout> CreatePipelineLayout(const vk::raii::Device& logicalDevice);

	CreationResult<vk::raii::Pipeline> CreateGraphicsPipeline(const vk::raii::Device& logicalDevice,
															  const vk::raii::PipelineLayout& pipelineLayout,
															  vk::Format colorAttachmentFormat);

	CreationResult<BufferResource> CreateBufferResource(const vk::raii::PhysicalDevice& physicalDevice,
														const vk::raii::Device& logicalDevice,
														vk::DeviceSize size,
														vk::BufferUsageFlags usage,
														vk::MemoryPropertyFlags properties);

	CreationResult<void> UploadToDeviceMemory(const vk::raii::DeviceMemory& deviceMemory, std::span<const std::byte> data);
	CreationResult<void> CopyBuffer(const vk::raii::Device& logicalDevice,
									const vk::raii::Queue& queue,
									const vk::raii::CommandPool& commandPool,
									const vk::raii::Buffer& srcBuffer,
									const vk::raii::Buffer& dstBuffer,
									const vk::DeviceSize size);

	CreationResult<vk::raii::CommandPool> CreateCommandPool(const vk::raii::Device& logicalDevice, std::uint32_t queueFamilyIndex);

	CreationResult<std::vector<vk::raii::CommandBuffer>> AllocateCommandBuffers(const vk::raii::Device& logicalDevice,
																				const vk::raii::CommandPool& commandPool,
																				std::uint32_t commandBufferCount);

	CreationResult<std::vector<vk::raii::Semaphore>> CreateSemaphores(const vk::raii::Device& logicalDevice, std::size_t count);

	CreationResult<std::vector<vk::raii::Fence>> CreateSignaledFences(const vk::raii::Device& logicalDevice, std::size_t count);
}
