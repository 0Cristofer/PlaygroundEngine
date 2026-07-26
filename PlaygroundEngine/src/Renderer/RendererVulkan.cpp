module;

#include "PlaygroundEngine/Log.h"

#include <vulkan/vulkan.h>

module PlaygroundEngine.RendererVulkan;

import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;
import PlaygroundEngine.Files;

import vulkan;
import PlaygroundEngine.Paths;

namespace PgE
{
	constexpr int MAX_FRAMES_IN_FLIGHT = 2;

	struct SwapChainResources
	{
		vk::raii::SwapchainKHR SwapChain;
		std::vector<vk::Image> Images;
		std::vector<vk::raii::ImageView> ImageViews;
		vk::Extent2D Extent;
	};

	std::expected<std::vector<vk::raii::ImageView>, RendererError<RendererCreationErrorKind>> CreateImageViews(
		const vk::raii::Device& logicalDevice, const vk::SurfaceFormatKHR& swapChainSurfaceFormat, const std::vector<vk::Image>& swapChainImages);

	std::expected<SwapChainResources, RendererError<RendererCreationErrorKind>> CreateSwapChainResources(
		const vk::raii::PhysicalDevice& physicalDevice,
		const vk::raii::Device& logicalDevice,
		const vk::raii::SurfaceKHR& surface,
		const FramebufferSize framebufferSize,
		const vk::SurfaceFormatKHR& swapChainSurfaceFormat)
	{
		// Capabilities are re-queried on every call: a resize moves currentExtent and the min/max
		// extents the new swap chain has to satisfy, so cached values from creation are stale here.

		std::expected<vk::SurfaceCapabilitiesKHR, vk::Result> surfaceCapabilitiesResult = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
		std::expected<std::vector<vk::PresentModeKHR>, vk::Result> availablePresentModesResult = physicalDevice.getSurfacePresentModesKHR(*surface);
		if (!surfaceCapabilitiesResult || !availablePresentModesResult)
		{
			const std::string message =
				!surfaceCapabilitiesResult ? ToString(surfaceCapabilitiesResult.error()) : ToString(availablePresentModesResult.error());
			return std::unexpected(RendererError(RendererCreationErrorKind::SurfaceCapabilitiesFormatsError, message));
		}
		const vk::SurfaceCapabilitiesKHR& surfaceCapabilities = surfaceCapabilitiesResult.value();

		const vk::PresentModeKHR foundPresentMode =
			std::ranges::any_of(availablePresentModesResult.value(),
								[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; })
				? vk::PresentModeKHR::eMailbox
				: vk::PresentModeKHR::eFifo;

		vk::Extent2D swapChainExtent{.width = std::clamp<std::uint32_t>(framebufferSize.Width, surfaceCapabilities.minImageExtent.width,
																		surfaceCapabilities.maxImageExtent.width),
									 .height = std::clamp<std::uint32_t>(framebufferSize.Height, surfaceCapabilities.minImageExtent.height,
																		 surfaceCapabilities.maxImageExtent.height)};

		unsigned minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if (0 < surfaceCapabilities.maxImageCount && surfaceCapabilities.maxImageCount < minImageCount)
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface = *surface,
													   .minImageCount = minImageCount,
													   .imageFormat = swapChainSurfaceFormat.format,
													   .imageColorSpace = swapChainSurfaceFormat.colorSpace,
													   .imageExtent = swapChainExtent,
													   .imageArrayLayers = 1,
													   .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
													   .imageSharingMode = vk::SharingMode::eExclusive,
													   .preTransform = surfaceCapabilities.currentTransform,
													   .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
													   .presentMode = foundPresentMode,
													   .clipped = true};

		std::expected<vk::raii::SwapchainKHR, vk::Result> swapChainResult = logicalDevice.createSwapchainKHR(swapChainCreateInfo);
		if (!swapChainResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::SwapChainCreationError, ToString(swapChainResult.error())));
		}
		vk::raii::SwapchainKHR& swapChain = swapChainResult.value();

		std::expected<std::vector<vk::Image>, vk::Result> swapChainImagesResult = swapChain.getImages();
		if (!swapChainImagesResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::SwapChainImagesCreationError, ToString(swapChainImagesResult.error())));
		}
		std::vector<vk::Image>& swapChainImages = swapChainImagesResult.value();

		std::expected<std::vector<vk::raii::ImageView>, RendererError<RendererCreationErrorKind>> swapChainImageViewsResult =
			CreateImageViews(logicalDevice, swapChainSurfaceFormat, swapChainImages);
		if (!swapChainImageViewsResult)
		{
			return std::unexpected(swapChainImageViewsResult.error());
		}

		return SwapChainResources{.SwapChain = std::move(swapChain),
								  .Images = std::move(swapChainImages),
								  .ImageViews = std::move(swapChainImageViewsResult.value()),
								  .Extent = swapChainExtent};
	}

	std::expected<std::vector<vk::raii::ImageView>, RendererError<RendererCreationErrorKind>> CreateImageViews(
		const vk::raii::Device& logicalDevice, const vk::SurfaceFormatKHR& swapChainSurfaceFormat, const std::vector<vk::Image>& swapChainImages)
	{
		vk::ImageViewCreateInfo imageViewCreateInfo{
			.viewType = vk::ImageViewType::e2D,
			.format = swapChainSurfaceFormat.format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

		std::vector<vk::raii::ImageView> swapChainImageViews;
		for (const vk::Image& image : swapChainImages)
		{
			imageViewCreateInfo.image = image;
			std::expected<vk::raii::ImageView, vk::Result> imageViewResult = logicalDevice.createImageView(imageViewCreateInfo);
			if (!imageViewResult)
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::ImageViewCreationError, ToString(imageViewResult.error())));
			}

			swapChainImageViews.push_back(std::move(imageViewResult.value()));
		}

		return swapChainImageViews;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanRendererDebugCallback(const vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
																		const vk::DebugUtilsMessageTypeFlagsEXT type,
																		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
																		[[maybe_unused]] void* pUserData)
	{
		switch (severity)
		{
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
			PGE_LOG(Trace, "validation layer: type {} msg: {}", to_string(type), pCallbackData->pMessage);
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
			PGE_LOG(Info, "validation layer: type {} msg: {}", to_string(type), pCallbackData->pMessage);
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
			PGE_LOG(Warn, "validation layer: type {} msg: {}", to_string(type), pCallbackData->pMessage);
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
			PGE_LOG(Error, "validation layer: type {} msg: {}", to_string(type), pCallbackData->pMessage);
			break;
		default:
			PGE_LOG(Info, "validation layer: type {} msg: {}", to_string(type), pCallbackData->pMessage);
			break;
		}

		return vk::False;
	}

	std::expected<std::unique_ptr<RendererVulkan>, RendererError<RendererCreationErrorKind>> RendererVulkan::Create(
		const RendererSpecification& specification, const Window& window)
	{
		vk::raii::Context context;

		// Instance creation

		vk::ApplicationInfo appInfo{.pApplicationName = specification.ApplicationName.c_str(),
									.applicationVersion = vk::makeVersion(1, 0, 0),
									.pEngineName = specification.EngineName.c_str(),
									.engineVersion = vk::makeVersion(1, 0, 0),
									.apiVersion = vk::ApiVersion14};

		std::expected<std::span<const char* const>, VulkanWindowError> requiredWindowExtensionsResult = window.GetRequiredVulkanExtensions();
		if (!requiredWindowExtensionsResult)
		{
			return std::unexpected(
				RendererError(RendererCreationErrorKind::UnableToRequestExtensions, ToString(requiredWindowExtensionsResult.error())));
		}
		std::span<const char* const> requiredWindowExtensions = requiredWindowExtensionsResult.value();

		std::expected<std::vector<vk::ExtensionProperties>, vk::Result> availableExtensionsResult = context.enumerateInstanceExtensionProperties();
		if (!availableExtensionsResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ExtensionsEnumerationError, ToString(availableExtensionsResult.error())));
		}
		const std::vector<vk::ExtensionProperties>& availableExtensions = availableExtensionsResult.value();

		std::vector<const char*> requiredExtraExtensions;
#if defined(PGE_DEV)
		requiredExtraExtensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif

		std::vector<const char*> requiredExtensions;
		requiredExtensions.reserve(requiredExtraExtensions.size() + requiredWindowExtensions.size());
		requiredExtensions.insert(requiredExtensions.end(), requiredWindowExtensions.begin(), requiredWindowExtensions.end());
		requiredExtensions.insert(requiredExtensions.end(), requiredExtraExtensions.begin(), requiredExtraExtensions.end());

		for (const char* const& requiredExtension : requiredExtensions)
		{
			if (std::ranges::none_of(availableExtensions, [&requiredExtension](const vk::ExtensionProperties& providedExtension) {
					return requiredExtension == std::string_view(providedExtension.extensionName);
				}))
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::ExtensionUnavailable, requiredExtension));
			}
		}

		std::vector<const char*> requiredValidationLayers;
#if defined(PGE_DEV)
		requiredValidationLayers = {"VK_LAYER_KHRONOS_validation"};
#endif

		std::expected<std::vector<vk::LayerProperties>, vk::Result> availableValidationLayersResult = context.enumerateInstanceLayerProperties();
		if (!availableValidationLayersResult)
		{
			return std::unexpected(
				RendererError(RendererCreationErrorKind::ValidationLayersEnumerationError, ToString(availableValidationLayersResult.error())));
		}
		const std::vector<vk::LayerProperties>& availableValidationLayers = availableValidationLayersResult.value();

		for (const char* const& requiredValidationLayer : requiredValidationLayers)
		{
			if (std::ranges::none_of(availableValidationLayers, [&requiredValidationLayer](const vk::LayerProperties& providedLayer) {
					return requiredValidationLayer == std::string_view(providedLayer.layerName);
				}))
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::ValidationLayerUnavailable, requiredValidationLayer));
			}
		}

#if defined(PGE_DEV)
		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
															vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
															vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
														   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
														   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{
			.messageSeverity = severityFlags, .messageType = messageTypeFlags, .pfnUserCallback = &VulkanRendererDebugCallback};
#endif

		vk::InstanceCreateInfo instanceCreateInfo{
#if defined(PGE_DEV)
			.pNext = &debugUtilsMessengerCreateInfo,
#endif
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<std::uint32_t>(requiredValidationLayers.size()),
			.ppEnabledLayerNames = requiredValidationLayers.data(),
			.enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size()),
			.ppEnabledExtensionNames = requiredExtensions.data(),
		};

		std::expected<vk::raii::Instance, vk::Result> instanceResult = context.createInstance(instanceCreateInfo);
		if (!instanceResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::InstanceCreationError, ToString(instanceResult.error())));
		}
		vk::raii::Instance& instance = instanceResult.value();

#if defined(PGE_DEV)
		std::expected<vk::raii::DebugUtilsMessengerEXT, vk::Result> debugMessengerResult =
			instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfo);
		if (!debugMessengerResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DebugMessengerCreationError, ToString(debugMessengerResult.error())));
		}
		vk::raii::DebugUtilsMessengerEXT& debugMessenger = debugMessengerResult.value();
#endif

		std::expected<VkSurfaceKHR, VulkanWindowError> surfaceCreationResult = window.CreateVulkanSurface(*instance);
		if (!surfaceCreationResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::UnableToCreateWindowSurface, ToString(surfaceCreationResult.error())));
		}
		auto surface = vk::raii::SurfaceKHR(instance, surfaceCreationResult.value());

		// Physical device selection

		auto physicalDevicesResult = instance.enumeratePhysicalDevices();
		if (!physicalDevicesResult)
		{
			return std::unexpected(
				RendererError(RendererCreationErrorKind::PhysicalDevicesEnumerationError, ToString(physicalDevicesResult.error())));
		}
		std::vector<vk::raii::PhysicalDevice>& physicalDevices = physicalDevicesResult.value();

		if (physicalDevices.empty())
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::NoPhysicalDevices));
		}

		std::vector requiredDeviceExtensions = {vk::KHRSwapchainExtensionName};

		std::vector<vk::raii::PhysicalDevice> suitableDevices;
		for (const vk::raii::PhysicalDevice& physicalDevice : physicalDevices)
		{
			const bool supportsVulkan13 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
			const std::vector<vk::QueueFamilyProperties>& queueFamilies = physicalDevice.getQueueFamilyProperties();
			bool supportsGraphics = std::ranges::any_of(queueFamilies, [](const vk::QueueFamilyProperties& queueFamilyProperty) {
				return !!(queueFamilyProperty.queueFlags & vk::QueueFlagBits::eGraphics);
			});

			std::expected<std::vector<vk::ExtensionProperties>, vk::Result> availableDeviceExtensionsResult =
				physicalDevice.enumerateDeviceExtensionProperties();
			if (!availableDeviceExtensionsResult)
			{
				PGE_LOG(Info, "Couldn't enumerate device extensions. Device: {}, Error: {}", physicalDevice.getProperties().deviceName,
						ToString(availableDeviceExtensionsResult.error()));
				continue;
			}
			const std::vector<vk::ExtensionProperties>& availableDeviceExtensions = availableDeviceExtensionsResult.value();
			bool supportsAllRequiredExtensions =
				std::ranges::all_of(requiredDeviceExtensions, [&availableDeviceExtensions](const auto& requiredDeviceExtension) {
					return std::ranges::any_of(availableDeviceExtensions, [requiredDeviceExtension](const auto& availableDeviceExtension) {
						return requiredDeviceExtension == std::string_view(availableDeviceExtension.extensionName);
					});
				});

			auto features = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
														vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
			bool supportsRequiredFeatures = features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
											features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
											features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

			if (supportsVulkan13 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures)
			{
				suitableDevices.push_back(physicalDevice);
			}
		}

		if (suitableDevices.empty())
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::NoSuitablePhysicalDevices));
		}

		// Logical device setup

		vk::raii::PhysicalDevice& physicalDevice = suitableDevices.front();

		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
		std::optional<std::uint32_t> queueIndex;
		for (std::uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
		{
			if (queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
			{
				// found a queue family that supports both graphics and present
				queueIndex = qfpIndex;
				break;
			}
		}
		if (!queueIndex.has_value())
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::SuitableQueueNotFound));
		}

		float queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex.value(), .queueCount = 1, .pQueuePriorities = &queuePriority};

		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features,
						   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
			featureChain = {
				{},													  // vk::PhysicalDeviceFeatures2
				{.shaderDrawParameters = true},						  // vk::PhysicalDeviceVulkan11Features
				{.synchronization2 = true, .dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
				{.extendedDynamicState = true}						  // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
			};
		std::vector requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

		vk::DeviceCreateInfo deviceCreateInfo{.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
											  .queueCreateInfoCount = 1,
											  .pQueueCreateInfos = &deviceQueueCreateInfo,
											  .enabledExtensionCount = static_cast<std::uint32_t>(requiredDeviceExtension.size()),
											  .ppEnabledExtensionNames = requiredDeviceExtension.data()};

		std::expected<vk::raii::Device, vk::Result> deviceResult = physicalDevice.createDevice(deviceCreateInfo);
		if (!deviceResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::LogicalDeviceCreationError, ToString(deviceResult.error())));
		}

		vk::raii::Device& logicalDevice = deviceResult.value();
		vk::raii::Queue queue = logicalDevice.getQueue(queueIndex.value(), 0);

		std::expected<std::vector<vk::SurfaceFormatKHR>, vk::Result> availableFormatsResult = physicalDevice.getSurfaceFormatsKHR(*surface);
		std::expected<std::vector<vk::PresentModeKHR>, vk::Result> availablePresentModesResult = physicalDevice.getSurfacePresentModesKHR(*surface);

		if (!availableFormatsResult || !availablePresentModesResult)
		{
			std::string message =
				!availableFormatsResult ? ToString(availableFormatsResult.error()) : ToString(availablePresentModesResult.error());
			return std::unexpected(RendererError(RendererCreationErrorKind::SurfaceCapabilitiesFormatsError, message));
		}

		std::vector<vk::SurfaceFormatKHR>& availableFormats = availableFormatsResult.value();
		std::vector<vk::PresentModeKHR>& availablePresentModes = availablePresentModesResult.value();

		const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format) {
			return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
		});
		vk::SurfaceFormatKHR swapChainSurfaceFormat = formatIt != availableFormats.end() ? *formatIt : availableFormats[0];

		if (!std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }))
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::UnavailablePresentMode,
												 std::format("Available present modes: {}", availablePresentModes.size())));
		}

		std::expected<SwapChainResources, RendererError<RendererCreationErrorKind>> swapChainResult =
			CreateSwapChainResources(physicalDevice, logicalDevice, surface, window.GetFramebufferSize(), swapChainSurfaceFormat);
		if (!swapChainResult)
		{
			return std::unexpected(swapChainResult.error());
		}

		vk::raii::SwapchainKHR& swapChain = swapChainResult->SwapChain;
		std::vector<vk::Image>& swapChainImages = swapChainResult->Images;
		std::vector<vk::raii::ImageView>& swapChainImageViews = swapChainResult->ImageViews;
		vk::Extent2D swapChainExtent = swapChainResult->Extent;

		std::expected<std::filesystem::path, PathError> executableDirectoryResult = GetExecutableDirectory();
		if (!executableDirectoryResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ShaderLoadError, ToString(executableDirectoryResult.error())));
		}

		std::expected<std::vector<std::byte>, FileError> shaderLoadResult =
			ReadBinaryFile(executableDirectoryResult.value() / "Shaders" / "shaders.spv");
		if (!shaderLoadResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ShaderLoadError, ToString(shaderLoadResult.error())));
		}
		std::vector<std::byte>& shaderBin = shaderLoadResult.value();

		vk::ShaderModuleCreateInfo shaderModuleCreateInfo{.codeSize = shaderBin.size(),
														  .pCode = reinterpret_cast<const std::uint32_t*>(shaderBin.data())};
		std::expected<vk::raii::ShaderModule, vk::Result> shaderModuleResult = logicalDevice.createShaderModule(shaderModuleCreateInfo);
		if (!shaderModuleResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ShaderModuleCreateError, ToString(shaderModuleResult.error())));
		}
		vk::raii::ShaderModule& shaderModule = shaderModuleResult.value();

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
			.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		std::vector dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
														.pDynamicStates = dynamicStates.data()};

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

		vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

		vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
															.rasterizerDiscardEnable = vk::False,
															.polygonMode = vk::PolygonMode::eFill,
															.cullMode = vk::CullModeFlagBits::eBack,
															.frontFace = vk::FrontFace::eClockwise,
															.depthBiasEnable = vk::False,
															.lineWidth = 1.0f};

		vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};
		vk::PipelineColorBlendAttachmentState colorBlendAttachment{.blendEnable = vk::False,
																   .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
																					 vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

		vk::PipelineColorBlendStateCreateInfo colorBlending{
			.logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};

		std::expected<vk::raii::PipelineLayout, vk::Result> pipelineLayoutResult = logicalDevice.createPipelineLayout(pipelineLayoutInfo);
		if (!pipelineLayoutResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::PipelineLayoutCreateError, ToString(pipelineLayoutResult.error())));
		}
		vk::raii::PipelineLayout& pipelineLayout = pipelineLayoutResult.value();

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
			{.stageCount = 2,
			 .pStages = shaderStages,
			 .pVertexInputState = &vertexInputInfo,
			 .pInputAssemblyState = &inputAssembly,
			 .pViewportState = &viewportState,
			 .pRasterizationState = &rasterizer,
			 .pMultisampleState = &multisampling,
			 .pColorBlendState = &colorBlending,
			 .pDynamicState = &dynamicState,
			 .layout = pipelineLayout,
			 .renderPass = nullptr},
			{.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

		auto graphicsPipelineResult = logicalDevice.createGraphicsPipeline(nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
		if (!graphicsPipelineResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::PipelineCreateError, ToString(graphicsPipelineResult.error())));
		}
		vk::raii::Pipeline& graphicsPipeline = graphicsPipelineResult.value();

		vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = queueIndex.value()};
		std::expected<vk::raii::CommandPool, vk::Result> commandPoolResult = logicalDevice.createCommandPool(poolInfo);
		if (!commandPoolResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CommandPoolCreateError, ToString(commandPoolResult.error())));
		}
		vk::raii::CommandPool& commandPool = commandPoolResult.value();

		vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
		std::expected<std::vector<vk::raii::CommandBuffer>, vk::Result> commandBuffersResult = logicalDevice.allocateCommandBuffers(allocInfo);
		if (!commandBuffersResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CommandBufferCreateError, ToString(commandBuffersResult.error())));
		}
		std::vector<vk::raii::CommandBuffer>& commandBuffers = commandBuffersResult.value();

		std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
		for (std::size_t i = 0; i < swapChainImages.size(); i++)
		{
			std::expected<vk::raii::Semaphore, vk::Result> renderFinishedSemaphoreResult = logicalDevice.createSemaphore({});
			if (!renderFinishedSemaphoreResult)
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::SemaphoreCreateError, ToString(renderFinishedSemaphoreResult.error())));
			}
			vk::raii::Semaphore& renderFinishedSemaphore = renderFinishedSemaphoreResult.value();
			renderFinishedSemaphores.push_back(std::move(renderFinishedSemaphore));
		}

		std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
		std::vector<vk::raii::Fence> inFlightFences;
		for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			std::expected<vk::raii::Semaphore, vk::Result> presentCompleteSemaphoreResult = logicalDevice.createSemaphore({});
			if (!presentCompleteSemaphoreResult)
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::SemaphoreCreateError, ToString(presentCompleteSemaphoreResult.error())));
			}
			vk::raii::Semaphore& presentCompleteSemaphore = presentCompleteSemaphoreResult.value();
			presentCompleteSemaphores.push_back(std::move(presentCompleteSemaphore));

			std::expected<vk::raii::Fence, vk::Result> inFlightFenceResult = logicalDevice.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
			if (!inFlightFenceResult)
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::FenceCreateError, ToString(inFlightFenceResult.error())));
			}
			vk::raii::Fence& inFlightFence = inFlightFenceResult.value();
			inFlightFences.push_back(std::move(inFlightFence));
		}

#if defined(PGE_DEV)
		return std::unique_ptr<RendererVulkan>(
			new RendererVulkan(std::move(context), std::move(instance), std::move(debugMessenger), std::move(surface), std::move(physicalDevice),
							   std::move(logicalDevice), std::move(queue), std::move(swapChain), std::move(swapChainImages),
							   std::move(swapChainSurfaceFormat), std::move(swapChainExtent), std::move(swapChainImageViews),
							   std::move(pipelineLayout), std::move(graphicsPipeline), std::move(commandPool), std::move(commandBuffers),
							   std::move(presentCompleteSemaphores), std::move(renderFinishedSemaphores), std::move(inFlightFences)));
#else
		return std::unique_ptr<RendererVulkan>(new RendererVulkan(
			std::move(context), std::move(instance), nullptr, std::move(surface), std::move(physicalDevice), std::move(logicalDevice),
			std::move(queue), std::move(swapChain), std::move(swapChainImages), std::move(swapChainSurfaceFormat), std::move(swapChainExtent),
			std::move(swapChainImageViews), std::move(pipelineLayout), std::move(graphicsPipeline), std::move(commandPool), std::move(commandBuffers),
			std::move(presentCompleteSemaphores), std::move(renderFinishedSemaphores), std::move(inFlightFences)));
#endif
	}

	void RendererVulkan::Teardown() const
	{
		[[maybe_unused]] auto waitResult = _logicalDevice.waitIdle();
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> RendererVulkan::DrawFrame(const FramebufferSize framebufferSize)
	{
		// A minimised window reports a zero framebuffer, and no swap chain can be built for one.

		if (framebufferSize.Width == 0 || framebufferSize.Height == 0)
		{
			return {};
		}

		if (const vk::Result fenceResult = _logicalDevice.waitForFences(*_inFlightFences[_frameIndex], vk::True, UINT64_MAX); fenceResult != vk::Result::eSuccess)
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

		// Reset only once the frame is certain to submit. Resetting before the acquire would leave
		// the fence unsignalled on the early return above, deadlocking the next wait on it.

		if (const std::expected<void, vk::Result> fenceResetResult = _logicalDevice.resetFences(*_inFlightFences[_frameIndex]); !fenceResetResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::FenceResetError, ToString(fenceResetResult)));
		}

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

		_frameIndex = (_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

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

		std::expected<SwapChainResources, RendererError<RendererCreationErrorKind>> swapChainResult =
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
			_renderFinishedSemaphores.clear();
			for (std::size_t imageIndex = 0; imageIndex < _swapChainImages.size(); imageIndex++)
			{
				std::expected<vk::raii::Semaphore, vk::Result> semaphoreResult = _logicalDevice.createSemaphore({});
				if (!semaphoreResult)
				{
					return std::unexpected(RendererError(RendererRenderErrorKind::SwapChainRecreationError, ToString(semaphoreResult.error())));
				}

				_renderFinishedSemaphores.push_back(std::move(semaphoreResult.value()));
			}
		}

		return {};
	}

	void RendererVulkan::TransitionImageLayout(const std::uint32_t imageIndex,
											   const vk::ImageLayout oldLayout,
											   const vk::ImageLayout newLayout,
											   const vk::AccessFlags2 srcAccessMask,
											   const vk::AccessFlags2 dstAccessMask,
											   const vk::PipelineStageFlags2 srcStageMask,
											   const vk::PipelineStageFlags2 dstStageMask) const
	{
		vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = _swapChainImages[imageIndex],
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
		const vk::DependencyInfo dependencyInfo = {.dependencyFlags = {}, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};

		_commandBuffers[_frameIndex].pipelineBarrier2(dependencyInfo);
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> RendererVulkan::RecordCommandBuffer(const std::uint32_t imageIndex) const
	{
		if (std::expected<void, vk::Result> beginResult = _commandBuffers[_frameIndex].begin({}); !beginResult)
		{
			return std::unexpected(RendererError(RendererRenderErrorKind::CommandBufferBeginError, ToString(beginResult.error())));
		}

		// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
		TransitionImageLayout(imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
							  {},												  // srcAccessMask (no need to wait for previous operations)
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

		_commandBuffers[_frameIndex].setViewport(
			0, vk::Viewport(0.0f, 0.0f, static_cast<float>(_swapChainExtent.width), static_cast<float>(_swapChainExtent.height), 0.0f, 1.0f));
		_commandBuffers[_frameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _swapChainExtent));

		_commandBuffers[_frameIndex].draw(3, 1, 0, 0);

		_commandBuffers[_frameIndex].endRendering();

		// After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
		TransitionImageLayout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
							  vk::AccessFlagBits2::eColorAttachmentWrite,		  // srcAccessMask
							  {},												  // dstAccessMask
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
