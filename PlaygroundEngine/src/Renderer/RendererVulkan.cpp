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
	constexpr int MaxFramesInFlight = 2;
	constexpr std::array RequiredDeviceExtensions = {vk::KHRSwapchainExtensionName};

	template <typename T>
	using CreationResult = std::expected<T, RendererError<RendererCreationErrorKind>>;

	struct SwapChainResources
	{
		vk::raii::SwapchainKHR SwapChain;
		std::vector<vk::Image> Images;
		std::vector<vk::raii::ImageView> ImageViews;
		vk::Extent2D Extent;
	};

#if defined(PGE_DEV)
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

	vk::DebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
	{
		constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
																	  vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
																	  vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
																	 vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
																	 vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

		return vk::DebugUtilsMessengerCreateInfoEXT{
			.messageSeverity = severityFlags, .messageType = messageTypeFlags, .pfnUserCallback = &VulkanRendererDebugCallback};
	}
#endif

	CreationResult<std::vector<const char*>> CollectRequiredInstanceExtensions(const vk::raii::Context& context, const Window& window)
	{
		std::expected<std::span<const char* const>, VulkanWindowError> requiredWindowExtensionsResult = window.GetRequiredVulkanExtensions();
		if (!requiredWindowExtensionsResult)
		{
			return std::unexpected(
				RendererError(RendererCreationErrorKind::UnableToRequestExtensions, ToString(requiredWindowExtensionsResult.error())));
		}
		const std::span<const char* const> requiredWindowExtensions = requiredWindowExtensionsResult.value();

		std::expected<std::vector<vk::ExtensionProperties>, vk::Result> availableExtensionsResult = context.enumerateInstanceExtensionProperties();
		if (!availableExtensionsResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ExtensionsEnumerationError, ToString(availableExtensionsResult.error())));
		}
		const std::vector<vk::ExtensionProperties>& availableExtensions = availableExtensionsResult.value();

		std::vector<const char*> requiredExtensions(requiredWindowExtensions.begin(), requiredWindowExtensions.end());
#if defined(PGE_DEV)
		requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif

		for (const char* const requiredExtension : requiredExtensions)
		{
			if (std::ranges::none_of(availableExtensions, [requiredExtension](const vk::ExtensionProperties& availableExtension) {
					return requiredExtension == std::string_view(availableExtension.extensionName);
				}))
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::ExtensionUnavailable, requiredExtension));
			}
		}

		return requiredExtensions;
	}

	CreationResult<std::vector<const char*>> CollectRequiredValidationLayers(const vk::raii::Context& context)
	{
		std::vector<const char*> requiredValidationLayers;
#if defined(PGE_DEV)
		requiredValidationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

		std::expected<std::vector<vk::LayerProperties>, vk::Result> availableLayersResult = context.enumerateInstanceLayerProperties();
		if (!availableLayersResult)
		{
			return std::unexpected(
				RendererError(RendererCreationErrorKind::ValidationLayersEnumerationError, ToString(availableLayersResult.error())));
		}
		const std::vector<vk::LayerProperties>& availableLayers = availableLayersResult.value();

		for (const char* const requiredLayer : requiredValidationLayers)
		{
			if (std::ranges::none_of(availableLayers, [requiredLayer](const vk::LayerProperties& availableLayer) {
					return requiredLayer == std::string_view(availableLayer.layerName);
				}))
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::ValidationLayerUnavailable, requiredLayer));
			}
		}

		return requiredValidationLayers;
	}

	CreationResult<vk::raii::Instance> CreateInstance(const vk::raii::Context& context,
													  const RendererSpecification& specification,
													  const Window& window)
	{
		const vk::ApplicationInfo applicationInfo{.pApplicationName = specification.ApplicationName.c_str(),
												  .applicationVersion = vk::makeVersion(1, 0, 0),
												  .pEngineName = specification.EngineName.c_str(),
												  .engineVersion = vk::makeVersion(1, 0, 0),
												  .apiVersion = vk::ApiVersion14};

		CreationResult<std::vector<const char*>> requiredExtensionsResult = CollectRequiredInstanceExtensions(context, window);
		if (!requiredExtensionsResult)
		{
			return std::unexpected(requiredExtensionsResult.error());
		}
		const std::vector<const char*>& requiredExtensions = requiredExtensionsResult.value();

		CreationResult<std::vector<const char*>> requiredValidationLayersResult = CollectRequiredValidationLayers(context);
		if (!requiredValidationLayersResult)
		{
			return std::unexpected(requiredValidationLayersResult.error());
		}
		const std::vector<const char*>& requiredValidationLayers = requiredValidationLayersResult.value();

#if defined(PGE_DEV)
		// Chained into the instance so the validation layer also reports faults raised during
		// instance creation and destruction, which the standalone messenger does not span.

		const vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = MakeDebugMessengerCreateInfo();
#endif

		const vk::InstanceCreateInfo instanceCreateInfo{
#if defined(PGE_DEV)
			.pNext = &debugUtilsMessengerCreateInfo,
#endif
			.pApplicationInfo = &applicationInfo,
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

		return std::move(instanceResult.value());
	}

	CreationResult<vk::raii::DebugUtilsMessengerEXT> CreateDebugMessenger([[maybe_unused]] const vk::raii::Instance& instance)
	{
#if defined(PGE_DEV)
		std::expected<vk::raii::DebugUtilsMessengerEXT, vk::Result> debugMessengerResult =
			instance.createDebugUtilsMessengerEXT(MakeDebugMessengerCreateInfo());
		if (!debugMessengerResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DebugMessengerCreationError, ToString(debugMessengerResult.error())));
		}

		return std::move(debugMessengerResult.value());
#else
		return vk::raii::DebugUtilsMessengerEXT(nullptr);
#endif
	}

	CreationResult<vk::raii::SurfaceKHR> CreateSurface(const vk::raii::Instance& instance, const Window& window)
	{
		std::expected<VkSurfaceKHR, VulkanWindowError> surfaceResult = window.CreateVulkanSurface(*instance);
		if (!surfaceResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::UnableToCreateWindowSurface, ToString(surfaceResult.error())));
		}

		return vk::raii::SurfaceKHR(instance, surfaceResult.value());
	}

	bool IsPhysicalDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice)
	{
		const bool supportsVulkan13 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

		const std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();
		const bool supportsGraphics = std::ranges::any_of(queueFamilies, [](const vk::QueueFamilyProperties& queueFamilyProperty) {
			return !!(queueFamilyProperty.queueFlags & vk::QueueFlagBits::eGraphics);
		});

		std::expected<std::vector<vk::ExtensionProperties>, vk::Result> availableExtensionsResult =
			physicalDevice.enumerateDeviceExtensionProperties();
		if (!availableExtensionsResult)
		{
			PGE_LOG(Info, "Couldn't enumerate device extensions. Device: {}, Error: {}", physicalDevice.getProperties().deviceName,
					ToString(availableExtensionsResult.error()));
			return false;
		}
		const std::vector<vk::ExtensionProperties>& availableExtensions = availableExtensionsResult.value();

		const bool supportsAllRequiredExtensions =
			std::ranges::all_of(RequiredDeviceExtensions, [&availableExtensions](const char* const requiredExtension) {
				return std::ranges::any_of(availableExtensions, [requiredExtension](const vk::ExtensionProperties& availableExtension) {
					return requiredExtension == std::string_view(availableExtension.extensionName);
				});
			});

		const auto features = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
														  vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		const bool supportsRequiredFeatures = features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
											  features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
											  features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		return supportsVulkan13 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
	}

	CreationResult<vk::raii::PhysicalDevice> SelectPhysicalDevice(const vk::raii::Instance& instance)
	{
		std::expected<std::vector<vk::raii::PhysicalDevice>, vk::Result> physicalDevicesResult = instance.enumeratePhysicalDevices();
		if (!physicalDevicesResult)
		{
			return std::unexpected(
				RendererError(RendererCreationErrorKind::PhysicalDevicesEnumerationError, ToString(physicalDevicesResult.error())));
		}
		const std::vector<vk::raii::PhysicalDevice>& physicalDevices = physicalDevicesResult.value();

		if (physicalDevices.empty())
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::NoPhysicalDevices));
		}

		const auto suitableDevice = std::ranges::find_if(physicalDevices, IsPhysicalDeviceSuitable);
		if (suitableDevice == physicalDevices.end())
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::NoSuitablePhysicalDevices));
		}

		return *suitableDevice;
	}

	CreationResult<std::uint32_t> FindGraphicsPresentQueueFamily(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface)
	{
		const std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
		for (std::uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyProperties.size(); queueFamilyIndex++)
		{
			if (!(queueFamilyProperties[queueFamilyIndex].queueFlags & vk::QueueFlagBits::eGraphics))
			{
				continue;
			}

			// A failed query is treated as "no present support" rather than an error: another
			// queue family may still serve, and an empty result surfaces as SuitableQueueNotFound.

			if (physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, *surface).value_or(vk::False))
			{
				return queueFamilyIndex;
			}
		}

		return std::unexpected(RendererError(RendererCreationErrorKind::SuitableQueueNotFound));
	}

	CreationResult<vk::raii::Device> CreateLogicalDevice(const vk::raii::PhysicalDevice& physicalDevice, const std::uint32_t queueFamilyIndex)
	{
		constexpr float queuePriority = 0.5f;
		const vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
			.queueFamilyIndex = queueFamilyIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};

		const vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features,
								 vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
			featureChain = {
				{},													  // vk::PhysicalDeviceFeatures2
				{.shaderDrawParameters = true},						  // vk::PhysicalDeviceVulkan11Features
				{.synchronization2 = true, .dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
				{.extendedDynamicState = true}						  // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
			};

		const vk::DeviceCreateInfo deviceCreateInfo{.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
													.queueCreateInfoCount = 1,
													.pQueueCreateInfos = &deviceQueueCreateInfo,
													.enabledExtensionCount = static_cast<std::uint32_t>(RequiredDeviceExtensions.size()),
													.ppEnabledExtensionNames = RequiredDeviceExtensions.data()};

		std::expected<vk::raii::Device, vk::Result> logicalDeviceResult = physicalDevice.createDevice(deviceCreateInfo);
		if (!logicalDeviceResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::LogicalDeviceCreationError, ToString(logicalDeviceResult.error())));
		}

		return std::move(logicalDeviceResult.value());
	}

	CreationResult<vk::SurfaceFormatKHR> SelectSurfaceFormat(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface)
	{
		std::expected<std::vector<vk::SurfaceFormatKHR>, vk::Result> availableFormatsResult = physicalDevice.getSurfaceFormatsKHR(*surface);
		if (!availableFormatsResult)
		{
			return std::unexpected(
				RendererError(RendererCreationErrorKind::SurfaceCapabilitiesFormatsError, ToString(availableFormatsResult.error())));
		}
		const std::vector<vk::SurfaceFormatKHR>& availableFormats = availableFormatsResult.value();

		if (availableFormats.empty())
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::SurfaceCapabilitiesFormatsError, "The surface reports no formats"));
		}

		const auto preferredFormat = std::ranges::find_if(availableFormats, [](const vk::SurfaceFormatKHR& format) {
			return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
		});

		return preferredFormat != availableFormats.end() ? *preferredFormat : availableFormats.front();
	}

	CreationResult<std::vector<vk::raii::ImageView>> CreateImageViews(const vk::raii::Device& logicalDevice,
																	  const vk::SurfaceFormatKHR& swapChainSurfaceFormat,
																	  const std::vector<vk::Image>& swapChainImages)
	{
		vk::ImageViewCreateInfo imageViewCreateInfo{
			.viewType = vk::ImageViewType::e2D,
			.format = swapChainSurfaceFormat.format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

		std::vector<vk::raii::ImageView> swapChainImageViews;
		swapChainImageViews.reserve(swapChainImages.size());

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

	CreationResult<SwapChainResources> CreateSwapChainResources(const vk::raii::PhysicalDevice& physicalDevice,
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

		CreationResult<std::vector<vk::raii::ImageView>> swapChainImageViewsResult =
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

	CreationResult<vk::raii::ShaderModule> LoadShaderModule(const vk::raii::Device& logicalDevice)
	{
		std::expected<std::filesystem::path, PathError> executableDirectoryResult = GetExecutableDirectory();
		if (!executableDirectoryResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ShaderLoadError, ToString(executableDirectoryResult.error())));
		}

		std::expected<std::vector<std::byte>, FileError> shaderBinaryResult =
			ReadBinaryFile(executableDirectoryResult.value() / "Shaders" / "shaders.spv");
		if (!shaderBinaryResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ShaderLoadError, ToString(shaderBinaryResult.error())));
		}
		const std::vector<std::byte>& shaderBinary = shaderBinaryResult.value();

		const vk::ShaderModuleCreateInfo shaderModuleCreateInfo{.codeSize = shaderBinary.size(),
																.pCode = reinterpret_cast<const std::uint32_t*>(shaderBinary.data())};
		std::expected<vk::raii::ShaderModule, vk::Result> shaderModuleResult = logicalDevice.createShaderModule(shaderModuleCreateInfo);
		if (!shaderModuleResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ShaderModuleCreateError, ToString(shaderModuleResult.error())));
		}

		return std::move(shaderModuleResult.value());
	}

	CreationResult<vk::raii::PipelineLayout> CreatePipelineLayout(const vk::raii::Device& logicalDevice)
	{
		constexpr vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};

		std::expected<vk::raii::PipelineLayout, vk::Result> pipelineLayoutResult = logicalDevice.createPipelineLayout(pipelineLayoutCreateInfo);
		if (!pipelineLayoutResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::PipelineLayoutCreateError, ToString(pipelineLayoutResult.error())));
		}

		return std::move(pipelineLayoutResult.value());
	}

	CreationResult<vk::raii::Pipeline> CreateGraphicsPipeline(const vk::raii::Device& logicalDevice,
															  const vk::raii::PipelineLayout& pipelineLayout,
															  const vk::Format colorAttachmentFormat)
	{
		// The shader module only has to outlive the pipeline creation call, so it stays local.

		CreationResult<vk::raii::ShaderModule> shaderModuleResult = LoadShaderModule(logicalDevice);
		if (!shaderModuleResult)
		{
			return std::unexpected(shaderModuleResult.error());
		}
		const vk::raii::ShaderModule& shaderModule = shaderModuleResult.value();

		const vk::PipelineShaderStageCreateInfo shaderStages[] = {
			{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"},
			{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"}};

		const std::vector dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		const vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
															  .pDynamicStates = dynamicStates.data()};

		constexpr vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

		constexpr vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

		constexpr vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

		constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
																	  .rasterizerDiscardEnable = vk::False,
																	  .polygonMode = vk::PolygonMode::eFill,
																	  .cullMode = vk::CullModeFlagBits::eBack,
																	  .frontFace = vk::FrontFace::eClockwise,
																	  .depthBiasEnable = vk::False,
																	  .lineWidth = 1.0f};

		constexpr vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1,
																	   .sampleShadingEnable = vk::False};

		constexpr vk::PipelineColorBlendAttachmentState colorBlendAttachment{.blendEnable = vk::False,
																			 .colorWriteMask =
																				 vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
																				 vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

		const vk::PipelineColorBlendStateCreateInfo colorBlending{
			.logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

		const vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
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
			{.colorAttachmentCount = 1, .pColorAttachmentFormats = &colorAttachmentFormat}};

		std::expected<vk::raii::Pipeline, vk::Result> graphicsPipelineResult =
			logicalDevice.createGraphicsPipeline(nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
		if (!graphicsPipelineResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::PipelineCreateError, ToString(graphicsPipelineResult.error())));
		}

		return std::move(graphicsPipelineResult.value());
	}

	CreationResult<vk::raii::CommandPool> CreateCommandPool(const vk::raii::Device& logicalDevice, const std::uint32_t queueFamilyIndex)
	{
		const vk::CommandPoolCreateInfo commandPoolCreateInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
															  .queueFamilyIndex = queueFamilyIndex};

		std::expected<vk::raii::CommandPool, vk::Result> commandPoolResult = logicalDevice.createCommandPool(commandPoolCreateInfo);
		if (!commandPoolResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CommandPoolCreateError, ToString(commandPoolResult.error())));
		}

		return std::move(commandPoolResult.value());
	}

	CreationResult<std::vector<vk::raii::CommandBuffer>> AllocateCommandBuffers(const vk::raii::Device& logicalDevice,
																				const vk::raii::CommandPool& commandPool)
	{
		const vk::CommandBufferAllocateInfo commandBufferAllocateInfo{
			.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MaxFramesInFlight};

		std::expected<std::vector<vk::raii::CommandBuffer>, vk::Result> commandBuffersResult =
			logicalDevice.allocateCommandBuffers(commandBufferAllocateInfo);
		if (!commandBuffersResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CommandBufferCreateError, ToString(commandBuffersResult.error())));
		}

		return std::move(commandBuffersResult.value());
	}

	CreationResult<std::vector<vk::raii::Semaphore>> CreateSemaphores(const vk::raii::Device& logicalDevice, const std::size_t count)
	{
		std::vector<vk::raii::Semaphore> semaphores;
		semaphores.reserve(count);

		for (std::size_t index = 0; index < count; index++)
		{
			std::expected<vk::raii::Semaphore, vk::Result> semaphoreResult = logicalDevice.createSemaphore({});
			if (!semaphoreResult)
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::SemaphoreCreateError, ToString(semaphoreResult.error())));
			}

			semaphores.push_back(std::move(semaphoreResult.value()));
		}

		return semaphores;
	}

	CreationResult<std::vector<vk::raii::Fence>> CreateSignaledFences(const vk::raii::Device& logicalDevice, const std::size_t count)
	{
		std::vector<vk::raii::Fence> fences;
		fences.reserve(count);

		for (std::size_t index = 0; index < count; index++)
		{
			std::expected<vk::raii::Fence, vk::Result> fenceResult = logicalDevice.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
			if (!fenceResult)
			{
				return std::unexpected(RendererError(RendererCreationErrorKind::FenceCreateError, ToString(fenceResult.error())));
			}

			fences.push_back(std::move(fenceResult.value()));
		}

		return fences;
	}

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

		CreationResult<vk::raii::PhysicalDevice> physicalDeviceResult = SelectPhysicalDevice(instance);
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

		CreationResult<vk::raii::Device> logicalDeviceResult = CreateLogicalDevice(physicalDevice, queueFamilyIndex);
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

		CreationResult<vk::raii::PipelineLayout> pipelineLayoutResult = CreatePipelineLayout(logicalDevice);
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

		CreationResult<std::vector<vk::raii::CommandBuffer>> commandBuffersResult = AllocateCommandBuffers(logicalDevice, commandPool);
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
			swapChain.Extent, std::move(swapChain.ImageViews), std::move(pipelineLayout), std::move(graphicsPipeline), std::move(commandPool),
			std::move(commandBuffers), std::move(presentCompleteSemaphores), std::move(renderFinishedSemaphores), std::move(inFlightFences)));
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
