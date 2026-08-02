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

	struct UniformBufferResource
	{
		BufferResource Buffer;
		void* BufferMapped;
	};

	struct ImageResource
	{
		vk::raii::Image Image;
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
	CreationResult<vk::raii::PipelineLayout> CreatePipelineLayout(const vk::raii::Device& logicalDevice,
																  const vk::raii::DescriptorSetLayout& descriptorSetLayout);
	CreationResult<vk::raii::DescriptorSetLayout> CreateDescriptorSetLayout(const vk::raii::Device& logicalDevice);
	CreationResult<vk::raii::Pipeline> CreateGraphicsPipeline(const vk::raii::Device& logicalDevice,
															  const vk::raii::PipelineLayout& pipelineLayout,
															  vk::Format colorAttachmentFormat,
															  vk::Format depthAttachmentFormat,
															  vk::SampleCountFlagBits sampleCount);
	CreationResult<BufferResource> CreateBufferResource(const vk::raii::PhysicalDevice& physicalDevice,
														const vk::raii::Device& logicalDevice,
														vk::DeviceSize size,
														vk::BufferUsageFlags usage,
														vk::MemoryPropertyFlags properties);
	CreationResult<void> UploadToDeviceMemory(const vk::raii::DeviceMemory& deviceMemory, std::span<const std::byte> data);

	// Staging buffer, upload, device-local buffer, copy. The staging buffer dies with the call.

	CreationResult<BufferResource> CreateDeviceLocalBuffer(const vk::raii::PhysicalDevice& physicalDevice,
														   const vk::raii::Device& logicalDevice,
														   const vk::raii::Queue& queue,
														   const vk::raii::CommandPool& commandPool,
														   std::span<const std::byte> data,
														   vk::BufferUsageFlags usage);
	CreationResult<void> CopyBuffer(const vk::raii::Device& logicalDevice,
									const vk::raii::Queue& queue,
									const vk::raii::CommandPool& commandPool,
									const vk::raii::Buffer& srcBuffer,
									const vk::raii::Buffer& dstBuffer,
									vk::DeviceSize size);
	CreationResult<vk::raii::CommandPool> CreateCommandPool(const vk::raii::Device& logicalDevice, std::uint32_t queueFamilyIndex);
	CreationResult<UniformBufferResource> CreateUniformBuffer(const vk::raii::PhysicalDevice& physicalDevice,
															  const vk::raii::Device& logicalDevice,
															  vk::DeviceSize bufferSize);
	CreationResult<vk::raii::DescriptorPool> CreateDescriptorPool(const vk::raii::Device& logicalDevice, std::uint32_t descriptorCount);
	CreationResult<std::vector<vk::raii::DescriptorSet>> CreateDescriptorSets(const vk::raii::Device& logicalDevice,
																			  const vk::raii::DescriptorSetLayout& descriptorSetLayout,
																			  const vk::raii::DescriptorPool& descriptorPool,
																			  std::uint32_t descriptorSetCount);
	CreationResult<std::vector<vk::raii::CommandBuffer>> AllocateCommandBuffers(const vk::raii::Device& logicalDevice,
																				const vk::raii::CommandPool& commandPool,
																				std::uint32_t commandBufferCount);
	CreationResult<std::vector<vk::raii::Semaphore>> CreateSemaphores(const vk::raii::Device& logicalDevice, std::size_t count);
	CreationResult<std::vector<vk::raii::Fence>> CreateSignaledFences(const vk::raii::Device& logicalDevice, std::size_t count);

	// Yields the image with its full mip chain already generated and every level left in
	// eShaderReadOnlyOptimal, alongside the level count the image view has to cover.

	CreationResult<std::tuple<ImageResource, std::uint32_t>> CreateTextureImage(const vk::raii::PhysicalDevice& physicalDevice,
																				const vk::raii::Device& logicalDevice,
																				const vk::raii::Queue& queue,
																				const vk::raii::CommandPool& commandPool,
																				std::string_view textureFileName);
	CreationResult<vk::raii::ImageView> CreateImageView(
		const vk::raii::Device& logicalDevice, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectMask, std::uint32_t mipLevels);
	CreationResult<vk::raii::Sampler> CreateTextureSampler(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Device& logicalDevice);
	CreationResult<vk::Format> FindDepthFormat(const vk::raii::PhysicalDevice& physicalDevice);

	// The highest count colour and depth attachments both support, since a pipeline needs one figure
	// for all of its attachments.

	vk::SampleCountFlagBits GetMaxUsableSampleCount(const vk::raii::PhysicalDevice& physicalDevice);

	CreationResult<std::tuple<ImageResource, vk::raii::ImageView>> CreateDepthResources(const vk::raii::PhysicalDevice& physicalDevice,
																						const vk::raii::Device& logicalDevice,
																						vk::Extent2D extent,
																						vk::Format depthFormat,
																						vk::SampleCountFlagBits sampleCount);

	// The multisampled image the pipeline renders into, resolved down to a swap chain image at the
	// end of the render. Transient: nothing reads it once the resolve has run.

	CreationResult<std::tuple<ImageResource, vk::raii::ImageView>> CreateMultisampleColorResources(const vk::raii::PhysicalDevice& physicalDevice,
																								   const vk::raii::Device& logicalDevice,
																								   vk::Extent2D extent,
																								   vk::Format colorFormat,
																								   vk::SampleCountFlagBits sampleCount);
	CreationResult<ImageResource> CreateImage(const vk::raii::PhysicalDevice& physicalDevice,
											  const vk::raii::Device& logicalDevice,
											  std::uint32_t width,
											  std::uint32_t height,
											  std::uint32_t mipLevels,
											  vk::SampleCountFlagBits sampleCount,
											  vk::Format format,
											  vk::ImageTiling tiling,
											  vk::ImageUsageFlags usage,
											  vk::MemoryPropertyFlags properties);
	CreationResult<vk::raii::CommandBuffer> BeginSingleTimeCommands(const vk::raii::Device& logicalDevice, const vk::raii::CommandPool& commandPool);
	CreationResult<void> EndSingleTimeCommands(const vk::raii::Queue& queue, vk::raii::CommandBuffer&& commandBuffer);

	void TransitionImageLayout(const vk::raii::CommandBuffer& commandBuffer,
							   vk::Image image,
							   vk::ImageLayout oldLayout,
							   vk::ImageLayout newLayout,
							   vk::AccessFlags2 srcAccessMask,
							   vk::AccessFlags2 dstAccessMask,
							   vk::PipelineStageFlags2 srcStageMask,
							   vk::PipelineStageFlags2 dstStageMask,
							   const vk::ImageSubresourceRange& subresourceRange);

	// Fills levels 1..mipLevels-1 by halving blits from the level above and leaves every level in
	// eShaderReadOnlyOptimal. Level 0 must already hold the source image in eTransferDstOptimal.

	void GenerateMipmaps(
		const vk::raii::CommandBuffer& commandBuffer, vk::Image image, std::int32_t width, std::int32_t height, std::uint32_t mipLevels);
	void CopyBufferToImage(const vk::raii::CommandBuffer& commandBuffer,
						   const vk::raii::Buffer& buffer,
						   const vk::raii::Image& image,
						   std::uint32_t width,
						   std::uint32_t height);
}
