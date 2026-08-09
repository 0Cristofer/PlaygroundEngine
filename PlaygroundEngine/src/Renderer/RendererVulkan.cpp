module;

#include "PlaygroundEngine/Log.h"

#include <vulkan/vulkan.h>
#include <imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

module PlaygroundEngine.Renderer.Vulkan;

import std;
import PlaygroundEngine.Reflection;
import PlaygroundEngine.Paths;
import PlaygroundEngine.Files;
import PlaygroundEngine.Image;
import PlaygroundEngine.Log;
import PlaygroundEngine.Model;
import PlaygroundEngine.Renderer.Vertex;

import vulkan;
import :VulkanTypes;
import :VulkanUtils;

namespace PgE
{
	constexpr std::uint32_t MaxFramesInFlight = 2;
	constexpr std::array RequiredDeviceExtensions = {vk::KHRSwapchainExtensionName};
	// Sized for the font atlas plus room for a handful of panel-owned textures; the ImGui backend
	// grows nothing on its own, so this is the ceiling on what the debug UI can bind at once.
	constexpr std::uint32_t DebugUiDescriptorPoolSize = 16;
	constexpr vk::DeviceSize DebugUiMinimumAllocationSize = 1024 * 1024;

	constexpr std::string_view PlaceholderTextureFileName = "viking_room.png";
	constexpr std::string_view PlaceholderModelFileName = "viking_room.obj";

	static CreationResult<Mesh> LoadMesh(const std::string_view modelFileName)
	{
		const std::expected<std::filesystem::path, PathError> executableDirectoryResult = GetExecutableDirectory();
		if (!executableDirectoryResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ModelLoadError, ToString(executableDirectoryResult.error())));
		}

		const std::expected<std::string, FileError> objectTextResult = ReadTextFile(executableDirectoryResult.value() / "Models" / modelFileName);
		if (!objectTextResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ModelLoadError, ToString(objectTextResult.error())));
		}

		std::expected<Mesh, ModelError> meshResult = ParseWavefrontMesh(objectTextResult.value());
		if (!meshResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ModelLoadError, ToString(meshResult.error())));
		}

		return std::move(meshResult.value());
	}

	static std::vector<Vertex> ToRendererVertices(const Mesh& mesh)
	{
		// Wavefront carries no vertex colour, and the fragment shader samples the texture rather than
		// interpolating one, so white leaves the attribute inert.

		std::vector<Vertex> vertices;
		vertices.reserve(mesh.Vertices.size());

		for (const MeshVertex& meshVertex : mesh.Vertices)
		{
			vertices.push_back(Vertex{.Pos = meshVertex.Position, .Color = {1.0f, 1.0f, 1.0f}, .TexCoord = meshVertex.TextureCoordinate});
		}

		return vertices;
	}

	std::expected<std::unique_ptr<RendererVulkan>, RendererError<RendererCreationErrorKind>> RendererVulkan::Create(
		const RendererSpecification& specification, const WindowServer& windowServer, const Window& window)
	{
		vk::raii::Context context;

		CreationResult<vk::raii::Instance> instanceResult = CreateInstance(context, specification, windowServer);
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

		CreationResult<vk::Format> depthFormatResult = FindDepthFormat(physicalDevice);
		if (!depthFormatResult)
		{
			return std::unexpected(depthFormatResult.error());
		}
		const vk::Format depthFormat = depthFormatResult.value();

		const vk::SampleCountFlagBits sampleCount = GetMaxUsableSampleCount(physicalDevice);

		CreationResult<std::tuple<ImageResource, vk::raii::ImageView>> multisampleColorResourcesResult =
			CreateMultisampleColorResources(physicalDevice, logicalDevice, swapChain.Extent, swapChainSurfaceFormat.format, sampleCount);
		if (!multisampleColorResourcesResult)
		{
			return std::unexpected(multisampleColorResourcesResult.error());
		}
		auto& [multisampleColorImageResource, multisampleColorImageView] = multisampleColorResourcesResult.value();

		CreationResult<std::tuple<ImageResource, vk::raii::ImageView>> depthResourcesResult =
			CreateDepthResources(physicalDevice, logicalDevice, swapChain.Extent, depthFormat, sampleCount);
		if (!depthResourcesResult)
		{
			return std::unexpected(depthResourcesResult.error());
		}
		auto& [depthImageResource, depthImageView] = depthResourcesResult.value();

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
			CreateGraphicsPipeline(logicalDevice, pipelineLayout, swapChainSurfaceFormat.format, depthFormat, sampleCount);
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

		CreationResult<Mesh> meshResult = LoadMesh(PlaceholderModelFileName);
		if (!meshResult)
		{
			return std::unexpected(meshResult.error());
		}
		const Mesh& mesh = meshResult.value();
		const std::vector<Vertex> vertices = ToRendererVertices(mesh);

		CreationResult<BufferResource> vertexBufferResourceResult = CreateDeviceLocalBuffer(
			physicalDevice, logicalDevice, queue, commandPool, std::as_bytes(std::span(vertices)), vk::BufferUsageFlagBits::eVertexBuffer);
		if (!vertexBufferResourceResult)
		{
			return std::unexpected(vertexBufferResourceResult.error());
		}
		BufferResource& vertexBufferResource = vertexBufferResourceResult.value();

		CreationResult<BufferResource> indexBufferResourceResult = CreateDeviceLocalBuffer(
			physicalDevice, logicalDevice, queue, commandPool, std::as_bytes(std::span(mesh.Indices)), vk::BufferUsageFlagBits::eIndexBuffer);
		if (!indexBufferResourceResult)
		{
			return std::unexpected(indexBufferResourceResult.error());
		}
		BufferResource& indexBufferResource = indexBufferResourceResult.value();

		CreationResult<std::tuple<ImageResource, std::uint32_t>> textureImageResourceResult =
			CreateTextureImage(physicalDevice, logicalDevice, queue, commandPool, PlaceholderTextureFileName);
		if (!textureImageResourceResult)
		{
			return std::unexpected(textureImageResourceResult.error());
		}
		auto& [textureImageResource, textureMipLevels] = textureImageResourceResult.value();

		CreationResult<vk::raii::ImageView> textureImageViewResult =
			CreateImageView(logicalDevice, *textureImageResource.Image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, textureMipLevels);
		if (!textureImageViewResult)
		{
			return std::unexpected(textureImageViewResult.error());
		}
		vk::raii::ImageView& textureImageView = textureImageViewResult.value();

		CreationResult<vk::raii::Sampler> textureSamplerResult = CreateTextureSampler(physicalDevice, logicalDevice);
		if (!textureSamplerResult)
		{
			return std::unexpected(textureSamplerResult.error());
		}
		vk::raii::Sampler& textureSampler = textureSamplerResult.value();

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
			vk::DescriptorImageInfo imageInfo{
				.sampler = textureSampler, .imageView = textureImageView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
			std::array<vk::WriteDescriptorSet, 2> descriptorWrites{{{.dstSet = descriptorSets[i],
																	 .dstBinding = 0,
																	 .dstArrayElement = 0,
																	 .descriptorCount = 1,
																	 .descriptorType = vk::DescriptorType::eUniformBuffer,
																	 .pBufferInfo = &bufferInfo},
																	{.dstSet = descriptorSets[i],
																	 .dstBinding = 1,
																	 .dstArrayElement = 0,
																	 .descriptorCount = 1,
																	 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
																	 .pImageInfo = &imageInfo}}};
			logicalDevice.updateDescriptorSets(descriptorWrites, {});
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

		std::unique_ptr<RendererVulkan> renderer(new RendererVulkan(
			std::move(context), std::move(instance), std::move(debugMessenger), std::move(surface), std::move(physicalDevice),
			std::move(logicalDevice), std::move(queue), std::move(swapChain.SwapChain), std::move(swapChain.Images), swapChainSurfaceFormat,
			swapChain.Extent, swapChain.SupportsTransferSource, std::move(swapChain.ImageViews), std::move(descriptorSetLayout),
			std::move(pipelineLayout), std::move(graphicsPipeline), std::move(commandPool), std::move(vertexBufferResource),
			std::move(indexBufferResource), static_cast<std::uint32_t>(mesh.Indices.size()), std::move(textureImageResource),
			std::move(textureImageView), std::move(textureSampler), sampleCount, std::move(multisampleColorImageResource),
			std::move(multisampleColorImageView), depthFormat, std::move(depthImageResource), std::move(depthImageView),
			std::move(uniformBufferResources), std::move(descriptorPool), std::move(descriptorSets), std::move(commandBuffers),
			std::move(presentCompleteSemaphores), std::move(renderFinishedSemaphores), std::move(inFlightFences)));

		// Deliberately not part of the result: the debug overlay is a development aid, so losing it
		// costs a log line rather than the run. The renderer reports the outcome through
		// IsDebugUiOverlayEnabled() and draws no overlay when it failed.

		if (specification.DebugUiOverlay)
		{
			renderer->InitializeDebugUiBackend(queueFamilyIndex);
		}

		return renderer;
	}

	void RendererVulkan::InitializeDebugUiBackend(const std::uint32_t queueFamilyIndex)
	{
		// The backend attaches to whatever context is current, and reads it before returning anything,
		// so with no context ImGui_ImplVulkan_Init aborts on an assert rather than reporting failure.

		if (ImGui::GetCurrentContext() == nullptr)
		{
			PGE_LOG(Error, "Debug UI overlay requested with no ImGui context: continuing without an overlay");
			return;
		}

		// The overlay renders straight onto the swap chain image, after the scene pass has resolved
		// into it, so the pipeline is built for that format at one sample rather than for the scene's
		// multisampled target.

		const auto colorAttachmentFormat = static_cast<VkFormat>(_swapChainSurfaceFormat.format);

		const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
																		.pNext = nullptr,
																		.viewMask = 0,
																		.colorAttachmentCount = 1,
																		.pColorAttachmentFormats = &colorAttachmentFormat,
																		.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
																		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED};

		const auto imageCount = static_cast<std::uint32_t>(_swapChainImages.size());

		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.ApiVersion = vk::ApiVersion14;
		initInfo.Instance = static_cast<VkInstance>(*_instance);
		initInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(*_physicalDevice);
		initInfo.Device = static_cast<VkDevice>(*_logicalDevice);
		initInfo.QueueFamily = queueFamilyIndex;
		initInfo.Queue = static_cast<VkQueue>(*_queue);

		// Leaving DescriptorPool null and giving a size instead makes the backend own its pool, so the
		// renderer's own pool stays sized for the scene alone.

		initInfo.DescriptorPoolSize = DebugUiDescriptorPoolSize;
		initInfo.MinImageCount = imageCount;
		initInfo.ImageCount = imageCount;
		initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
		initInfo.UseDynamicRendering = true;

		// Vulkan's best-practices validation warns about small dedicated allocations, and validation
		// errors are a hard gate here, so the backend is asked to allocate in larger blocks.

		initInfo.MinAllocationSize = DebugUiMinimumAllocationSize;

		if (!ImGui_ImplVulkan_Init(&initInfo))
		{
			PGE_LOG(Error, "ImGui Vulkan backend failed to initialize: continuing without an overlay");
			return;
		}

		_debugUiOverlayEnabled = true;
	}

	void RendererVulkan::Teardown() const
	{
		[[maybe_unused]] auto waitResult = _logicalDevice.waitIdle();

		// After the wait: the backend frees device resources the in-flight frames may still be using.

		if (_debugUiOverlayEnabled)
		{
			ImGui_ImplVulkan_Shutdown();
		}
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> RendererVulkan::DrawFrame(const PlatformEventRecord& platformEventRecord,
																						  const FramebufferSize framebufferSize,
																						  ImDrawData* debugUiDrawData)
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

		const auto currentFrameTime = std::chrono::steady_clock::now();
		const float deltaTimeSeconds = std::chrono::duration<float>(currentFrameTime - _previousFrameTime).count();
		_previousFrameTime = currentFrameTime;

		_cameraInput = ReadCameraInput(platformEventRecord, _cameraInput);
		MoveCamera(_cameraInput, deltaTimeSeconds);

		UpdateUniformBuffer(_frameIndex);

		if (const std::expected<void, vk::Result> resetCommandBufferResult = _commandBuffers[_frameIndex].reset(); !resetCommandBufferResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::UnableToResetCommandBuffer, ToString(resetCommandBufferResult)));
		}

		if (std::expected<void, RendererError<RendererRenderErrorKind>> recordCommandBufferResult = RecordCommandBuffer(imageIndex, debugUiDrawData);
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

		// Serviced after the submit and before the present, the one window where the frame is finished
		// and the image is still owned by the application.
		if (_pendingCapturePath)
		{
			const std::filesystem::path capturePath = *std::exchange(_pendingCapturePath, std::nullopt);

			if (const std::expected<void, vk::Result> waitResult = _logicalDevice.waitIdle(); !waitResult)
			{
				PGE_LOG(Error, "Frame capture to {} failed: device wait error {}", capturePath.display_string(), ToString(waitResult.error()));
			}
			else if (const CreationResult<void> captureResult = CaptureSwapChainImage(imageIndex, capturePath); !captureResult)
			{
				PGE_LOG(Error, "Frame capture to {} failed: {}", capturePath.display_string(), captureResult.error().Message());
			}
			else
			{
				PGE_LOG(Info, "Frame captured to {}", capturePath.display_string());
			}
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

	void RendererVulkan::RequestCapture(std::filesystem::path path)
	{
		// Only one request can be outstanding, and a caller learns the outcome by looking for the
		// file, so a displaced path would otherwise be waited on forever with nothing written.

		if (_pendingCapturePath)
		{
			PGE_LOG(Warn, "Frame capture to {} displaced by a newer request before any frame served it", _pendingCapturePath->display_string());
		}

		_pendingCapturePath = std::move(path);
	}

	CreationResult<void> RendererVulkan::CaptureSwapChainImage(const std::uint32_t imageIndex, const std::filesystem::path& path) const
	{
		if (!_swapChainSupportsTransferSource)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::SwapChainTransferSourceUnsupported,
												 "the surface does not support transfer-source swap chain images"));
		}

		// Resolved before the readback, not after: ReadImageToHost sizes its buffer for four bytes per
		// pixel, so a wider format has to be rejected before a copy is issued against it rather than
		// once the pixels are already in hand.

		// SelectSurfaceFormat prefers BGRA but falls back to whatever the surface offers first, so the
		// channel order is a runtime fact. Only the 8-bit four-channel families are handled; anything
		// else would need a real format conversion table.

		bool swapRedAndBlue = false;

		switch (_swapChainSurfaceFormat.format)
		{
		case vk::Format::eB8G8R8A8Unorm:
		case vk::Format::eB8G8R8A8Srgb:
			swapRedAndBlue = true;
			break;

		case vk::Format::eR8G8B8A8Unorm:
		case vk::Format::eR8G8B8A8Srgb:
			break;

		default:
			return std::unexpected(RendererError(RendererCreationErrorKind::CaptureFormatUnsupported,
												 std::string("unhandled swap chain format ") + to_string(_swapChainSurfaceFormat.format)));
		}

		// The presented image is the resolve target of the multisampled pass, so it already holds
		// exactly the pixels the compositor was handed. Nothing has to be re-rendered to capture it.

		CreationResult<std::vector<std::byte>> pixelsResult = ReadImageToHost(
			_physicalDevice, _logicalDevice, _queue, _commandPool, _swapChainImages[imageIndex], vk::ImageLayout::ePresentSrcKHR, _swapChainExtent);
		if (!pixelsResult)
		{
			return std::unexpected(pixelsResult.error());
		}
		std::vector<std::byte>& pixels = pixelsResult.value();

		if (swapRedAndBlue)
		{
			for (std::size_t offset = 0; offset < pixels.size(); offset += Image::BytesPerPixel)
			{
				std::swap(pixels[offset], pixels[offset + 2]);
			}
		}

		// Forced opaque rather than trusting the swap chain's alpha: compositeAlpha is eOpaque, so
		// nothing downstream of the render ever constrained that channel, and a PNG that honoured it
		// could come out invisible in a viewer.

		for (std::size_t offset = Image::BytesPerPixel - 1; offset < pixels.size(); offset += Image::BytesPerPixel)
		{
			pixels[offset] = std::byte{0xFF};
		}

		// An sRGB swap chain holds bytes that are already sRGB-encoded, which is what a PNG stores,
		// so there is no gamma step here for either format family.

		const std::expected<std::vector<std::byte>, ImageError> encodedResult =
			EncodeImagePng(Image{.Pixels = std::move(pixels), .Width = _swapChainExtent.width, .Height = _swapChainExtent.height});
		if (!encodedResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CaptureEncodeError, ToString(encodedResult.error())));
		}

		if (const std::expected<void, FileError> writeResult = WriteBinaryFile(path, encodedResult.value()); !writeResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CaptureWriteError, ToString(writeResult.error())));
		}

		return {};
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
		_swapChainSupportsTransferSource = swapChainResult->SupportsTransferSource;

		// The backend sizes its per-image buffers from this count, and a rebuilt swap chain can come
		// back with a different one.

		if (_debugUiOverlayEnabled)
		{
			ImGui_ImplVulkan_SetMinImageCount(static_cast<std::uint32_t>(_swapChainImages.size()));
		}

		CreationResult<std::tuple<ImageResource, vk::raii::ImageView>> multisampleColorResourcesResult =
			CreateMultisampleColorResources(_physicalDevice, _logicalDevice, _swapChainExtent, _swapChainSurfaceFormat.format, _sampleCount);
		if (!multisampleColorResourcesResult)
		{
			return std::unexpected(
				RendererError(RendererRenderErrorKind::SwapChainRecreationError, std::string(multisampleColorResourcesResult.error().Message())));
		}
		auto& [multisampleColorImageResource, multisampleColorImageView] = multisampleColorResourcesResult.value();

		_multisampleColorImageView = nullptr;
		_multisampleColorImageResource = std::move(multisampleColorImageResource);
		_multisampleColorImageView = std::move(multisampleColorImageView);

		CreationResult<std::tuple<ImageResource, vk::raii::ImageView>> depthResourcesResult =
			CreateDepthResources(_physicalDevice, _logicalDevice, _swapChainExtent, _depthFormat, _sampleCount);
		if (!depthResourcesResult)
		{
			return std::unexpected(
				RendererError(RendererRenderErrorKind::SwapChainRecreationError, std::string(depthResourcesResult.error().Message())));
		}
		auto& [depthImageResource, depthImageView] = depthResourcesResult.value();

		// The old view goes first: assigning over the image resource would otherwise destroy a
		// VkImage that the outgoing view still references.

		_depthImageView = nullptr;
		_depthImageResource = std::move(depthImageResource);
		_depthImageView = std::move(depthImageView);

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

		_ubo.Proj = glm::perspective(glm::radians(45.0f), static_cast<float>(_swapChainExtent.width) / static_cast<float>(_swapChainExtent.height),
									 0.1f, 10.0f);
		_ubo.Proj[1][1] *= -1;

		return {};
	}

	RendererVulkan::CameraInputState RendererVulkan::ReadCameraInput(const PlatformEventRecord& platformEventRecord, CameraInputState previousState)
	{
		for (const auto& event : platformEventRecord.GetEvents())
		{
			if (event.Type != PlatformEventType::KeyPressed && event.Type != PlatformEventType::KeyReleased)
			{
				continue;
			}

			const bool isPressed = event.Type == PlatformEventType::KeyPressed;
			switch (event.Code)
			{
			case InputCode::KeyW:
				previousState.MoveForward = isPressed;
				break;
			case InputCode::KeyS:
				previousState.MoveBackward = isPressed;
				break;
			case InputCode::KeyA:
				previousState.StrafeLeft = isPressed;
				break;
			case InputCode::KeyD:
				previousState.StrafeRight = isPressed;
				break;
			case InputCode::KeyLeft:
				previousState.TurnLeft = isPressed;
				break;
			case InputCode::KeyRight:
				previousState.TurnRight = isPressed;
				break;
			case InputCode::KeyUp:
				previousState.LookUp = isPressed;
				break;
			case InputCode::KeyDown:
				previousState.LookDown = isPressed;
				break;
			default:
				break;
			}
		}

		return previousState;
	}

	void RendererVulkan::MoveCamera(const CameraInputState cameraInput, const float deltaTimeSeconds)
	{
		constexpr float turnSpeed = 1.5f;

		// Pitch stops just short of straight up or down, where forward would become parallel to the
		// world up axis and the right-vector cross product would collapse to zero.

		constexpr float pitchLimit = 1.5f;

		const auto axisFromKeys = [](const bool positive, const bool negative) {
			return static_cast<float>(positive) - static_cast<float>(negative);
		};

		_cameraYaw += axisFromKeys(cameraInput.TurnRight, cameraInput.TurnLeft) * turnSpeed * deltaTimeSeconds;
		_cameraPitch =
			std::clamp(_cameraPitch + axisFromKeys(cameraInput.LookUp, cameraInput.LookDown) * turnSpeed * deltaTimeSeconds, -pitchLimit, pitchLimit);

		const glm::vec3 forward = GetCameraForward();
		const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

		const glm::vec3 movement = forward * axisFromKeys(cameraInput.MoveForward, cameraInput.MoveBackward) +
								   right * axisFromKeys(cameraInput.StrafeRight, cameraInput.StrafeLeft);

		if (glm::length(movement) > 0.0f)
		{
			constexpr float moveSpeed = 1.5f;
			_cameraPosition += glm::normalize(movement) * moveSpeed * deltaTimeSeconds;
		}

		RebuildViewMatrix();
	}

	glm::vec3 RendererVulkan::GetCameraForward() const
	{
		return glm::vec3(std::cos(_cameraPitch) * std::sin(_cameraYaw), std::sin(_cameraPitch), -std::cos(_cameraPitch) * std::cos(_cameraYaw));
	}

	void RendererVulkan::RebuildViewMatrix()
	{
		_ubo.View = lookAt(_cameraPosition, _cameraPosition + GetCameraForward(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	void RendererVulkan::UpdateUniformBuffer(const std::uint32_t frameIndex) const
	{
		std::memcpy(_uniformBufferResources[frameIndex].BufferMapped, &_ubo, sizeof(_ubo));
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> RendererVulkan::RecordCommandBuffer(const std::uint32_t imageIndex,
																									ImDrawData* debugUiDrawData) const
	{
		if (std::expected<void, vk::Result> beginResult = _commandBuffers[_frameIndex].begin({}); !beginResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::CommandBufferBeginError, ToString(beginResult.error())));
		}

		// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
		TransitionImageLayout(
			_commandBuffers[_frameIndex], _swapChainImages[imageIndex], vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eNone,							// srcAccessMask (no need to wait for previous operations)
			vk::AccessFlagBits2::eColorAttachmentWrite,			// dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
			{.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1});

		// The multisampled target is cleared on load and never stored, so eUndefined is the honest old
		// layout here for the same reason it is on the depth buffer.

		TransitionImageLayout(
			_commandBuffers[_frameIndex], *_multisampleColorImageResource.Image, vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite,			// srcAccessMask
			vk::AccessFlagBits2::eColorAttachmentWrite,			// dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
			{.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1});
		TransitionImageLayout(
			_commandBuffers[_frameIndex], *_depthImageResource.Image, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,												  // srcAccessMask
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,												  // dstAccessMask
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, // srcStage
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, // dstStage
			{.aspectMask = vk::ImageAspectFlagBits::eDepth, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1});

		// Rendering targets the multisampled image and resolves into the swap chain image as the render
		// ends, which is where dynamic rendering replaces a render pass's pResolveAttachments. storeOp
		// is eDontCare because only the resolved result is ever presented.

		constexpr vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachmentInfo = {.imageView = _multisampleColorImageView,
													  .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
													  .resolveMode = vk::ResolveModeFlagBits::eAverage,
													  .resolveImageView = _swapChainImageViews[imageIndex],
													  .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
													  .loadOp = vk::AttachmentLoadOp::eClear,
													  .storeOp = vk::AttachmentStoreOp::eDontCare,
													  .clearValue = clearColor};

		constexpr vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
		vk::RenderingAttachmentInfo depthAttachmentInfo = {.imageView = _depthImageView,
														   .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
														   .loadOp = vk::AttachmentLoadOp::eClear,
														   .storeOp = vk::AttachmentStoreOp::eDontCare,
														   .clearValue = clearDepth};

		const vk::RenderingInfo renderingInfo = {.renderArea = {.offset = {.x = 0, .y = 0}, .extent = _swapChainExtent},
												 .layerCount = 1,
												 .colorAttachmentCount = 1,
												 .pColorAttachments = &attachmentInfo,
												 .pDepthAttachment = &depthAttachmentInfo};

		_commandBuffers[_frameIndex].beginRendering(renderingInfo);

		_commandBuffers[_frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline);
		_commandBuffers[_frameIndex].bindVertexBuffers(0, *_vertexBufferResource.Buffer, {0});
		_commandBuffers[_frameIndex].bindIndexBuffer(*_indexBufferResource.Buffer, 0, vk::IndexType::eUint32);

		_commandBuffers[_frameIndex].setViewport(
			0, vk::Viewport(0.0f, 0.0f, static_cast<float>(_swapChainExtent.width), static_cast<float>(_swapChainExtent.height), 0.0f, 1.0f));
		_commandBuffers[_frameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _swapChainExtent));

		_commandBuffers[_frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipelineLayout, 0, *_descriptorSets[_frameIndex], nullptr);
		_commandBuffers[_frameIndex].drawIndexed(_indexCount, 1, 0, 0, 0);

		_commandBuffers[_frameIndex].endRendering();

		// The enabled check belongs here rather than at the caller: draw data handed to a renderer
		// whose overlay never came up is dropped, not a precondition violation.

		if (_debugUiOverlayEnabled && debugUiDrawData != nullptr)
		{
			RecordDebugUiOverlay(imageIndex, debugUiDrawData);
		}

		// After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
		TransitionImageLayout(
			_commandBuffers[_frameIndex], _swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,			// srcAccessMask
			vk::AccessFlagBits2::eNone,							// dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
			vk::PipelineStageFlagBits2::eBottomOfPipe,			// dstStage
			{.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1});

		if (std::expected<void, vk::Result> endResult = _commandBuffers[_frameIndex].end(); !endResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::CommandBufferEndError, ToString(endResult.error())));
		}

		return {};
	}

	void RendererVulkan::RecordDebugUiOverlay(const std::uint32_t imageIndex, ImDrawData* debugUiDrawData) const
	{
		// The scene resolves into the swap chain image as its rendering ends, and the overlay
		// loads that same image, so the two colour writes need ordering. Old and new layout are
		// equal on purpose: this is a barrier, not a transition.

		TransitionImageLayout(
			_commandBuffers[_frameIndex], _swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite,												// srcAccessMask
			vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite, // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,										// srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,										// dstStage
			{.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1});

		// A second rendering scope rather than a share of the scene's: the overlay is 2D, so it
		// wants the resolved single-sampled image directly. Drawing it in the scene's pass would
		// put UI text through MSAA and tie ImGui's pipeline to the scene's sample count.

		const vk::RenderingAttachmentInfo overlayAttachmentInfo = {.imageView = _swapChainImageViews[imageIndex],
																   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
																   .loadOp = vk::AttachmentLoadOp::eLoad,
																   .storeOp = vk::AttachmentStoreOp::eStore};

		const vk::RenderingInfo overlayRenderingInfo = {.renderArea = {.offset = {.x = 0, .y = 0}, .extent = _swapChainExtent},
														.layerCount = 1,
														.colorAttachmentCount = 1,
														.pColorAttachments = &overlayAttachmentInfo};

		_commandBuffers[_frameIndex].beginRendering(overlayRenderingInfo);

		ImGui_ImplVulkan_RenderDrawData(debugUiDrawData, *_commandBuffers[_frameIndex]);

		_commandBuffers[_frameIndex].endRendering();
	}

}
