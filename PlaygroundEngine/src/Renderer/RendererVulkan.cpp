module;

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

module PlaygroundEngine.Renderer.Vulkan;

import std;
import PlaygroundEngine.Reflection;
import PlaygroundEngine.Renderer.Vertex;

import vulkan;
import :VulkanTypes;
import :VulkanUtils;

namespace PgE
{
	constexpr std::uint32_t MaxFramesInFlight = 2;
	constexpr std::array RequiredDeviceExtensions = {vk::KHRSwapchainExtensionName};
	constexpr std::array Vertices = {
		Vertex{.Pos = {-0.5f, -0.5f}, .Color = {1.0f, 0.f, 0.f}}, Vertex{.Pos = {0.5f, -0.5f}, .Color = {0.0f, 1.0f, 0.0f}},
		Vertex{.Pos = {0.5f, 0.5f}, .Color = {0.0f, 0.0f, 1.0f}}, Vertex{.Pos = {-0.5f, 0.5f}, .Color = {0.0f, 1.0f, 1.0f}}};
	constexpr std::array<std::uint16_t, 6> Indices = {0, 1, 2, 2, 3, 0};
	constexpr std::string_view PlaceholderTextureFileName = "placeholder.png";

	std::expected<std::unique_ptr<RendererVulkan>, RendererError<RendererCreationErrorKind>> RendererVulkan::Create(
		const RendererSpecification& specification, const Window& window)
	{
		vk::raii::Context context;

		CreationResult<vk::raii::Instance> instanceResult = CreateInstance(context, specification, window);
		if (!instanceResult)
		{
			return std::unexpected(instanceResult.error());
		}
		vk::raii::Instance& instance = instanceResult.value();

		CreationResult<vk::raii::DebugUtilsMessengerEXT> debugMessengerResult = CreateDebugMessenger(instance);
		if (!debugMessengerResult)
		{
			return std::unexpected(debugMessengerResult.error());
		}
		vk::raii::DebugUtilsMessengerEXT& debugMessenger = debugMessengerResult.value();

		CreationResult<vk::raii::SurfaceKHR> surfaceResult = CreateSurface(instance, window);
		if (!surfaceResult)
		{
			return std::unexpected(surfaceResult.error());
		}
		vk::raii::SurfaceKHR& surface = surfaceResult.value();

		CreationResult<vk::raii::PhysicalDevice> physicalDeviceResult = SelectPhysicalDevice(instance, RequiredDeviceExtensions);
		if (!physicalDeviceResult)
		{
			return std::unexpected(physicalDeviceResult.error());
		}
		vk::raii::PhysicalDevice& physicalDevice = physicalDeviceResult.value();

		CreationResult<std::uint32_t> queueFamilyIndexResult = FindGraphicsPresentQueueFamily(physicalDevice, surface);
		if (!queueFamilyIndexResult)
		{
			return std::unexpected(queueFamilyIndexResult.error());
		}
		const std::uint32_t queueFamilyIndex = queueFamilyIndexResult.value();

		CreationResult<vk::raii::Device> logicalDeviceResult = CreateLogicalDevice(physicalDevice, queueFamilyIndex, RequiredDeviceExtensions);
		if (!logicalDeviceResult)
		{
			return std::unexpected(logicalDeviceResult.error());
		}
		vk::raii::Device& logicalDevice = logicalDeviceResult.value();

		vk::raii::Queue queue = logicalDevice.getQueue(queueFamilyIndex, 0);

		CreationResult<vk::SurfaceFormatKHR> surfaceFormatResult = SelectSurfaceFormat(physicalDevice, surface);
		if (!surfaceFormatResult)
		{
			return std::unexpected(surfaceFormatResult.error());
		}
		const vk::SurfaceFormatKHR swapChainSurfaceFormat = surfaceFormatResult.value();

		CreationResult<SwapChainResources> swapChainResult =
			CreateSwapChainResources(physicalDevice, logicalDevice, surface, window.GetFramebufferSize(), swapChainSurfaceFormat);
		if (!swapChainResult)
		{
			return std::unexpected(swapChainResult.error());
		}
		SwapChainResources& swapChain = swapChainResult.value();

		CreationResult<vk::raii::DescriptorSetLayout> descriptorSetLayoutResult = CreateDescriptorSetLayout(logicalDevice);
		if (!descriptorSetLayoutResult)
		{
			return std::unexpected(descriptorSetLayoutResult.error());
		}
		vk::raii::DescriptorSetLayout& descriptorSetLayout = descriptorSetLayoutResult.value();

		CreationResult<vk::raii::PipelineLayout> pipelineLayoutResult = CreatePipelineLayout(logicalDevice, descriptorSetLayout);
		if (!pipelineLayoutResult)
		{
			return std::unexpected(pipelineLayoutResult.error());
		}
		vk::raii::PipelineLayout& pipelineLayout = pipelineLayoutResult.value();

		CreationResult<vk::raii::Pipeline> graphicsPipelineResult =
			CreateGraphicsPipeline(logicalDevice, pipelineLayout, swapChainSurfaceFormat.format);
		if (!graphicsPipelineResult)
		{
			return std::unexpected(graphicsPipelineResult.error());
		}
		vk::raii::Pipeline& graphicsPipeline = graphicsPipelineResult.value();

		CreationResult<vk::raii::CommandPool> commandPoolResult = CreateCommandPool(logicalDevice, queueFamilyIndex);
		if (!commandPoolResult)
		{
			return std::unexpected(commandPoolResult.error());
		}
		vk::raii::CommandPool& commandPool = commandPoolResult.value();

		CreationResult<BufferResource> stagingVertexBufferResourceResult =
			CreateBufferResource(physicalDevice, logicalDevice, sizeof(Vertices), vk::BufferUsageFlagBits::eTransferSrc,
								 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		if (!stagingVertexBufferResourceResult)
		{
			return std::unexpected(stagingVertexBufferResourceResult.error());
		}
		BufferResource& stagingVertexBufferResource = stagingVertexBufferResourceResult.value();

		if (CreationResult<void> uploadResult = UploadToDeviceMemory(stagingVertexBufferResource.DeviceMemory, std::as_bytes(std::span(Vertices)));
			!uploadResult)
		{
			return std::unexpected(uploadResult.error());
		}

		CreationResult<BufferResource> vertexBufferResourceResult = CreateBufferResource(
			physicalDevice, logicalDevice, sizeof(Vertices), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		if (!vertexBufferResourceResult)
		{
			return std::unexpected(vertexBufferResourceResult.error());
		}
		BufferResource& vertexBufferResource = vertexBufferResourceResult.value();

		CreationResult<void> vertexBufferCopyResult =
			CopyBuffer(logicalDevice, queue, commandPool, stagingVertexBufferResource.Buffer, vertexBufferResource.Buffer, sizeof(Vertices));
		if (!vertexBufferCopyResult)
		{
			return std::unexpected(vertexBufferCopyResult.error());
		}

		CreationResult<BufferResource> stagingIndexBufferResourceResult =
			CreateBufferResource(physicalDevice, logicalDevice, sizeof(Indices), vk::BufferUsageFlagBits::eTransferSrc,
								 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		if (!stagingIndexBufferResourceResult)
		{
			return std::unexpected(stagingIndexBufferResourceResult.error());
		}
		BufferResource& stagingIndexBufferResource = stagingIndexBufferResourceResult.value();

		if (CreationResult<void> uploadResult = UploadToDeviceMemory(stagingIndexBufferResource.DeviceMemory, std::as_bytes(std::span(Indices)));
			!uploadResult)
		{
			return std::unexpected(uploadResult.error());
		}

		CreationResult<BufferResource> indexBufferResourceResult = CreateBufferResource(
			physicalDevice, logicalDevice, sizeof(Indices), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		if (!indexBufferResourceResult)
		{
			return std::unexpected(indexBufferResourceResult.error());
		}
		BufferResource& indexBufferResource = indexBufferResourceResult.value();

		CreationResult<void> indexBufferCopyResult =
			CopyBuffer(logicalDevice, queue, commandPool, stagingIndexBufferResource.Buffer, indexBufferResource.Buffer, sizeof(Indices));
		if (!indexBufferCopyResult)
		{
			return std::unexpected(indexBufferCopyResult.error());
		}

		CreationResult<ImageResource> textureImageResourceResult =
			CreateTextureImage(physicalDevice, logicalDevice, queue, commandPool, PlaceholderTextureFileName);
		if (!textureImageResourceResult)
		{
			return std::unexpected(textureImageResourceResult.error());
		}
		ImageResource& textureImageResource = textureImageResourceResult.value();

		std::vector<UniformBufferResource> uniformBufferResources;
		for (std::size_t i = 0; i < MaxFramesInFlight; ++i)
		{
			CreationResult<UniformBufferResource> uniformBufferResourceResult =
				CreateUniformBuffer(physicalDevice, logicalDevice, sizeof(UniformBufferObject));
			if (!uniformBufferResourceResult)
			{
				return std::unexpected(uniformBufferResourceResult.error());
			}
			uniformBufferResources.emplace_back(std::move(uniformBufferResourceResult.value()));
		}

		CreationResult<vk::raii::DescriptorPool> descriptorPoolResult = CreateDescriptorPool(logicalDevice, MaxFramesInFlight);
		if (!descriptorPoolResult)
		{
			return std::unexpected(descriptorPoolResult.error());
		}
		vk::raii::DescriptorPool& descriptorPool = descriptorPoolResult.value();

		CreationResult<std::vector<vk::raii::DescriptorSet>> descriptorSetsResult =
			CreateDescriptorSets(logicalDevice, descriptorSetLayout, descriptorPool, MaxFramesInFlight);
		if (!descriptorSetsResult)
		{
			return std::unexpected(descriptorSetsResult.error());
		}
		std::vector<vk::raii::DescriptorSet>& descriptorSets = descriptorSetsResult.value();

		for (std::size_t i = 0; i < MaxFramesInFlight; i++)
		{
			vk::DescriptorBufferInfo bufferInfo{.buffer = uniformBufferResources[i].Buffer.Buffer, .offset = 0, .range = sizeof(UniformBufferObject)};
			vk::WriteDescriptorSet descriptorWrite{.dstSet = descriptorSets[i],
												   .dstBinding = 0,
												   .dstArrayElement = 0,
												   .descriptorCount = 1,
												   .descriptorType = vk::DescriptorType::eUniformBuffer,
												   .pBufferInfo = &bufferInfo};
			logicalDevice.updateDescriptorSets(descriptorWrite, {});
		}

		CreationResult<std::vector<vk::raii::CommandBuffer>> commandBuffersResult =
			AllocateCommandBuffers(logicalDevice, commandPool, MaxFramesInFlight);
		if (!commandBuffersResult)
		{
			return std::unexpected(commandBuffersResult.error());
		}
		std::vector<vk::raii::CommandBuffer>& commandBuffers = commandBuffersResult.value();

		// One render-finished semaphore per swap chain image, since the present waits on the
		// semaphore belonging to the image it hands over, not to the frame in flight.

		CreationResult<std::vector<vk::raii::Semaphore>> renderFinishedSemaphoresResult = CreateSemaphores(logicalDevice, swapChain.Images.size());
		if (!renderFinishedSemaphoresResult)
		{
			return std::unexpected(renderFinishedSemaphoresResult.error());
		}
		std::vector<vk::raii::Semaphore>& renderFinishedSemaphores = renderFinishedSemaphoresResult.value();

		CreationResult<std::vector<vk::raii::Semaphore>> presentCompleteSemaphoresResult = CreateSemaphores(logicalDevice, MaxFramesInFlight);
		if (!presentCompleteSemaphoresResult)
		{
			return std::unexpected(presentCompleteSemaphoresResult.error());
		}
		std::vector<vk::raii::Semaphore>& presentCompleteSemaphores = presentCompleteSemaphoresResult.value();

		CreationResult<std::vector<vk::raii::Fence>> inFlightFencesResult = CreateSignaledFences(logicalDevice, MaxFramesInFlight);
		if (!inFlightFencesResult)
		{
			return std::unexpected(inFlightFencesResult.error());
		}
		std::vector<vk::raii::Fence>& inFlightFences = inFlightFencesResult.value();

		return std::unique_ptr<RendererVulkan>(new RendererVulkan(
			std::move(context), std::move(instance), std::move(debugMessenger), std::move(surface), std::move(physicalDevice),
			std::move(logicalDevice), std::move(queue), std::move(swapChain.SwapChain), std::move(swapChain.Images), swapChainSurfaceFormat,
			swapChain.Extent, std::move(swapChain.ImageViews), std::move(descriptorSetLayout), std::move(pipelineLayout), std::move(graphicsPipeline),
			std::move(commandPool), std::move(vertexBufferResource), std::move(indexBufferResource), std::move(textureImageResource),
			std::move(uniformBufferResources), std::move(descriptorPool), std::move(descriptorSets), std::move(commandBuffers),
			std::move(presentCompleteSemaphores), std::move(renderFinishedSemaphores), std::move(inFlightFences)));
	}

	void RendererVulkan::Teardown() const
	{
		[[maybe_unused]] auto waitResult = _logicalDevice.waitIdle();
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> RendererVulkan::DrawFrame(const FramebufferSize framebufferSize)
	{
		// A minimized window reports a zero framebuffer, and no swap chain can be built for one.

		if (framebufferSize.Width == 0 || framebufferSize.Height == 0)
		{
			return {};
		}

		if (const vk::Result fenceResult = _logicalDevice.waitForFences(*_inFlightFences[_frameIndex], vk::True, UINT64_MAX);
			fenceResult != vk::Result::eSuccess)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::FenceWaitError, ToString(fenceResult)));
		}

		auto [acquireNextImageResult, imageIndex] = _swapChain.acquireNextImage(UINT64_MAX, *_presentCompleteSemaphores[_frameIndex], nullptr);

		if (acquireNextImageResult == vk::Result::eErrorOutOfDateKHR)
		{
			return RecreateSwapChain(framebufferSize);
		}

		// eSuboptimalKHR still yields a usable image, so the frame proceeds and the swap chain is
		// rebuilt after the present instead.

		if (acquireNextImageResult != vk::Result::eSuccess && acquireNextImageResult != vk::Result::eSuboptimalKHR)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::UnableToAcquireSwapChainImage, ToString(acquireNextImageResult)));
		}

		// Reset only once the frame is certain to submit. Resetting before acquire would leave
		// the fence unsignalled on the early return above, deadlocking the next wait on it.

		if (const std::expected<void, vk::Result> fenceResetResult = _logicalDevice.resetFences(*_inFlightFences[_frameIndex]); !fenceResetResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::FenceResetError, ToString(fenceResetResult)));
		}

		UpdateUniformBuffer(_frameIndex);

		if (const std::expected<void, vk::Result> resetCommandBufferResult = _commandBuffers[_frameIndex].reset(); !resetCommandBufferResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::UnableToResetCommandBuffer, ToString(resetCommandBufferResult)));
		}

		if (std::expected<void, RendererError<RendererRenderErrorKind>> recordCommandBufferResult = RecordCommandBuffer(imageIndex);
			!recordCommandBufferResult)
		{
			return recordCommandBufferResult;
		}

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
										.pWaitSemaphores = &*_presentCompleteSemaphores[_frameIndex],
										.pWaitDstStageMask = &waitDestinationStageMask,
										.commandBufferCount = 1,
										.pCommandBuffers = &*_commandBuffers[_frameIndex],
										.signalSemaphoreCount = 1,
										.pSignalSemaphores = &*_renderFinishedSemaphores[imageIndex]};

		if (std::expected<void, vk::Result> submitResult = _queue.submit(submitInfo, *_inFlightFences[_frameIndex]); !submitResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::QueueSubmitError, ToString(submitResult.error())));
		}

		const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
												.pWaitSemaphores = &*_renderFinishedSemaphores[imageIndex],
												.swapchainCount = 1,
												.pSwapchains = &*_swapChain,
												.pImageIndices = &imageIndex};

		const vk::Result presentResult = _queue.presentKHR(presentInfoKHR);

		_frameIndex = (_frameIndex + 1) % MaxFramesInFlight;

		if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR || _framebufferResized)
		{
			_framebufferResized = false;
			return RecreateSwapChain(framebufferSize);
		}

		if (presentResult != vk::Result::eSuccess)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::QueuePresentError, ToString(presentResult)));
		}

		return {};
	}

	void RendererVulkan::NotifyFramebufferResized()
	{
		_framebufferResized = true;
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> RendererVulkan::RecreateSwapChain(const FramebufferSize framebufferSize)
	{
		if (const std::expected<void, vk::Result> waitResult = _logicalDevice.waitIdle(); !waitResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::DeviceWaitIdleError, ToString(waitResult.error())));
		}

		// The old swap chain is released before the new one is built. Keeping both alive would
		// require handing the old handle over as oldSwapchain, which the surface would otherwise
		// still consider in use.

		_swapChainImageViews.clear();
		_swapChainImages.clear();
		_swapChain = nullptr;

		CreationResult<SwapChainResources> swapChainResult =
			CreateSwapChainResources(_physicalDevice, _logicalDevice, _surface, framebufferSize, _swapChainSurfaceFormat);
		if (!swapChainResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::SwapChainRecreationError, std::string(swapChainResult.error().Message())));
		}

		_swapChain = std::move(swapChainResult->SwapChain);
		_swapChainImages = std::move(swapChainResult->Images);
		_swapChainImageViews = std::move(swapChainResult->ImageViews);
		_swapChainExtent = swapChainResult->Extent;

		// Render-finished semaphores are indexed by swap chain image, so a changed image count
		// would otherwise leave the vector too short to index during the next present.

		if (_renderFinishedSemaphores.size() != _swapChainImages.size())
		{
			// The current set stays alive until the replacement is complete, so a failed 'create'
			// leaves usable semaphores behind rather than an empty vector the next present indexes.

			CreationResult<std::vector<vk::raii::Semaphore>> semaphoresResult = CreateSemaphores(_logicalDevice, _swapChainImages.size());
			if (!semaphoresResult)
			{
				return std::unexpected(
					RendererError(RendererRenderErrorKind::SwapChainRecreationError, std::string(semaphoresResult.error().Message())));
			}

			_renderFinishedSemaphores = std::move(semaphoresResult.value());
		}

		return {};
	}

	void RendererVulkan::UpdateUniformBuffer(const std::uint32_t frameIndex) const
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		const auto currentTime = std::chrono::high_resolution_clock::now();
		const float timeSeconds = std::chrono::duration<float>(currentTime - startTime).count();

		UniformBufferObject ubo;
		ubo.Model = glm::rotate(glm::mat4(1.0f), timeSeconds * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.View = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.Proj = glm::perspective(glm::radians(45.0f), static_cast<float>(_swapChainExtent.width) / static_cast<float>(_swapChainExtent.height),
									0.1f, 10.0f);
		ubo.Proj[1][1] *= -1;
		std::memcpy(_uniformBufferResources[frameIndex].BufferMapped, &ubo, sizeof(ubo));
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> RendererVulkan::RecordCommandBuffer(const std::uint32_t imageIndex) const
	{
		if (std::expected<void, vk::Result> beginResult = _commandBuffers[_frameIndex].begin({}); !beginResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::CommandBufferBeginError, ToString(beginResult.error())));
		}

		// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
		TransitionImageLayout(_commandBuffers[_frameIndex], _swapChainImages[imageIndex], vk::ImageLayout::eUndefined,
							  vk::ImageLayout::eColorAttachmentOptimal,
							  vk::AccessFlagBits2::eNone,						  // srcAccessMask (no need to wait for previous operations)
							  vk::AccessFlagBits2::eColorAttachmentWrite,		  // dstAccessMask
							  vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
							  vk::PipelineStageFlagBits2::eColorAttachmentOutput  // dstStage
		);

		constexpr vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachmentInfo = {.imageView = _swapChainImageViews[imageIndex],
													  .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
													  .loadOp = vk::AttachmentLoadOp::eClear,
													  .storeOp = vk::AttachmentStoreOp::eStore,
													  .clearValue = clearColor};
		const vk::RenderingInfo renderingInfo = {.renderArea = {.offset = {.x = 0, .y = 0}, .extent = _swapChainExtent},
												 .layerCount = 1,
												 .colorAttachmentCount = 1,
												 .pColorAttachments = &attachmentInfo};

		_commandBuffers[_frameIndex].beginRendering(renderingInfo);

		_commandBuffers[_frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline);
		_commandBuffers[_frameIndex].bindVertexBuffers(0, *_vertexBufferResource.Buffer, {0});
		_commandBuffers[_frameIndex].bindIndexBuffer(*_indexBufferResource.Buffer, 0, vk::IndexType::eUint16);

		_commandBuffers[_frameIndex].setViewport(
			0, vk::Viewport(0.0f, 0.0f, static_cast<float>(_swapChainExtent.width), static_cast<float>(_swapChainExtent.height), 0.0f, 1.0f));
		_commandBuffers[_frameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _swapChainExtent));

		_commandBuffers[_frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipelineLayout, 0, *_descriptorSets[_frameIndex], nullptr);
		_commandBuffers[_frameIndex].drawIndexed(Indices.size(), 1, 0, 0, 0);

		_commandBuffers[_frameIndex].endRendering();

		// After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
		TransitionImageLayout(_commandBuffers[_frameIndex], _swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
							  vk::ImageLayout::ePresentSrcKHR,
							  vk::AccessFlagBits2::eColorAttachmentWrite,		  // srcAccessMask
							  vk::AccessFlagBits2::eNone,						  // dstAccessMask
							  vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
							  vk::PipelineStageFlagBits2::eBottomOfPipe			  // dstStage
		);

		if (std::expected<void, vk::Result> endResult = _commandBuffers[_frameIndex].end(); !endResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::CommandBufferEndError, ToString(endResult.error())));
		}

		return {};
	}

}
