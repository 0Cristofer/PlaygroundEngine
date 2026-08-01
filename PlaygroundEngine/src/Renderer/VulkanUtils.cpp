module;

#include "PlaygroundEngine/Log.h"

#include <vulkan/vulkan.h>

module PlaygroundEngine.Renderer.Vulkan;

import std;
import vulkan;
import PlaygroundEngine.Log;
import PlaygroundEngine.Window;
import PlaygroundEngine.Reflection;
import PlaygroundEngine.Paths;
import PlaygroundEngine.Files;
import PlaygroundEngine.Renderer.Vertex;
import :VulkanTypes;
import :VulkanUtils;
import PlaygroundEngine.Image;

namespace PgE
{
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

	static vk::DebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
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

	static CreationResult<std::vector<const char*>> CollectRequiredInstanceExtensions(const vk::raii::Context& context, const Window& window)
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

	static CreationResult<std::vector<const char*>> CollectRequiredValidationLayers(const vk::raii::Context& context)
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

	static bool IsPhysicalDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice, const std::span<const char* const> requiredExtensions)
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
			std::ranges::all_of(requiredExtensions, [&availableExtensions](const char* const requiredExtension) {
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

	CreationResult<vk::raii::PhysicalDevice> SelectPhysicalDevice(const vk::raii::Instance& instance,
																  const std::span<const char* const> requiredExtensions)
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

		const auto suitableDevice = std::ranges::find_if(physicalDevices, [requiredExtensions](const auto& physicalDevice) {
			return IsPhysicalDeviceSuitable(physicalDevice, requiredExtensions);
		});
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

	CreationResult<vk::raii::Device> CreateLogicalDevice(const vk::raii::PhysicalDevice& physicalDevice,
														 const std::uint32_t queueFamilyIndex,
														 const std::span<const char* const> requiredExtensions)
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
													.enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size()),
													.ppEnabledExtensionNames = requiredExtensions.data()};

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

	static CreationResult<std::vector<vk::raii::ImageView>> CreateImageViews(const vk::raii::Device& logicalDevice,
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

	CreationResult<vk::raii::PipelineLayout> CreatePipelineLayout(const vk::raii::Device& logicalDevice,
																  const vk::raii::DescriptorSetLayout& descriptorSetLayout)
	{
		const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
			.setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0};

		std::expected<vk::raii::PipelineLayout, vk::Result> pipelineLayoutResult = logicalDevice.createPipelineLayout(pipelineLayoutCreateInfo);
		if (!pipelineLayoutResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::PipelineLayoutCreateError, ToString(pipelineLayoutResult.error())));
		}

		return std::move(pipelineLayoutResult.value());
	}

	CreationResult<vk::raii::DescriptorSetLayout> CreateDescriptorSetLayout(const vk::raii::Device& logicalDevice)
	{
		vk::DescriptorSetLayoutBinding uboLayoutBinding{
			.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex};
		const vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &uboLayoutBinding};

		std::expected<vk::raii::DescriptorSetLayout, vk::Result> descriptorSetLayoutResult = logicalDevice.createDescriptorSetLayout(layoutInfo);
		if (!descriptorSetLayoutResult)
		{
			return std::unexpected(
				RendererError(RendererCreationErrorKind::DescriptorSetLayoutCreateError, ToString(descriptorSetLayoutResult.error())));
		}

		return std::move(descriptorSetLayoutResult.value());
	}

	static CreationResult<vk::raii::ShaderModule> LoadShaderModule(const vk::raii::Device& logicalDevice)
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

		const vk::VertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
		const std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions = Vertex::GetAttributeDescriptions();
		const vk::PipelineVertexInputStateCreateInfo vertexInputInfo{.vertexBindingDescriptionCount = 1,
																	 .pVertexBindingDescriptions = &bindingDescription,
																	 .vertexAttributeDescriptionCount =
																		 static_cast<std::uint32_t>(attributeDescriptions.size()),
																	 .pVertexAttributeDescriptions = attributeDescriptions.data()};

		constexpr vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
		constexpr vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};
		constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
																	  .rasterizerDiscardEnable = vk::False,
																	  .polygonMode = vk::PolygonMode::eFill,
																	  .cullMode = vk::CullModeFlagBits::eBack,
																	  .frontFace = vk::FrontFace::eCounterClockwise,
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

	static CreationResult<std::uint32_t> FindMemoryType(const vk::raii::PhysicalDevice& physicalDevice,
														const std::uint32_t typeFilter,
														const vk::MemoryPropertyFlags properties)
	{
		const vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

		for (std::uint32_t memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++)
		{
			const bool allowedByFilter = typeFilter & (1 << memoryTypeIndex);
			if (allowedByFilter && (memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & properties) == properties)
			{
				return memoryTypeIndex;
			}
		}

		return std::unexpected(RendererError(RendererCreationErrorKind::CantFindMemoryType));
	}

	CreationResult<BufferResource> CreateBufferResource(const vk::raii::PhysicalDevice& physicalDevice,
														const vk::raii::Device& logicalDevice,
														const vk::DeviceSize size,
														const vk::BufferUsageFlags usage,
														const vk::MemoryPropertyFlags properties)
	{
		const vk::BufferCreateInfo bufferCreateInfo{.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};

		std::expected<vk::raii::Buffer, vk::Result> bufferResult = logicalDevice.createBuffer(bufferCreateInfo);
		if (!bufferResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::BufferCreateError, ToString(bufferResult.error())));
		}
		vk::raii::Buffer& buffer = bufferResult.value();

		const vk::MemoryRequirements memoryRequirements = buffer.getMemoryRequirements();
		CreationResult<std::uint32_t> memoryTypeResult = FindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties);
		if (!memoryTypeResult)
		{
			return std::unexpected(memoryTypeResult.error());
		}

		const vk::MemoryAllocateInfo memoryAllocateInfo{.allocationSize = memoryRequirements.size, .memoryTypeIndex = memoryTypeResult.value()};
		std::expected<vk::raii::DeviceMemory, vk::Result> deviceMemoryResult = logicalDevice.allocateMemory(memoryAllocateInfo);
		if (!deviceMemoryResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DeviceMemoryAllocationError, ToString(deviceMemoryResult.error())));
		}
		vk::raii::DeviceMemory& deviceMemory = deviceMemoryResult.value();

		if (std::expected<void, vk::Result> bindResult = buffer.bindMemory(*deviceMemory, 0); !bindResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DeviceMemoryBindError, ToString(bindResult.error())));
		}

		return BufferResource{.Buffer = std::move(buffer), .DeviceMemory = std::move(deviceMemory)};
	}

	CreationResult<void> UploadToDeviceMemory(const vk::raii::DeviceMemory& deviceMemory, const std::span<const std::byte> data)
	{
		// Host-coherent memory only: there is no flush here, so a caller mapping non-coherent
		// memory would have to invalidate the range itself.

		std::expected<void*, vk::Result> mappedResult = deviceMemory.mapMemory(0, data.size_bytes());
		if (!mappedResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DeviceMemoryMapError, ToString(mappedResult.error())));
		}

		std::memcpy(mappedResult.value(), data.data(), data.size_bytes());
		deviceMemory.unmapMemory();

		return {};
	}

	CreationResult<void> CopyBuffer(const vk::raii::Device& logicalDevice,
									const vk::raii::Queue& queue,
									const vk::raii::CommandPool& commandPool,
									const vk::raii::Buffer& srcBuffer,
									const vk::raii::Buffer& dstBuffer,
									const vk::DeviceSize size)
	{
		CreationResult<vk::raii::CommandBuffer> copyCommandBufferResult = BeginSingleTimeCommands(logicalDevice, commandPool);
		if (!copyCommandBufferResult)
		{
			return std::unexpected(copyCommandBufferResult.error());
		}
		vk::raii::CommandBuffer& copyCommandBuffer = copyCommandBufferResult.value();

		copyCommandBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));

		if (CreationResult<void> endCommandBufferResult = EndSingleTimeCommands(queue, std::move(copyCommandBuffer)); !endCommandBufferResult)
		{
			return std::unexpected(endCommandBufferResult.error());
		}

		return {};
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

	CreationResult<UniformBufferResource> CreateUniformBuffer(const vk::raii::PhysicalDevice& physicalDevice,
															  const vk::raii::Device& logicalDevice,
															  const vk::DeviceSize bufferSize)
	{
		CreationResult<BufferResource> bufferResult =
			CreateBufferResource(physicalDevice, logicalDevice, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
								 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		if (!bufferResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::UniformBufferCreateError, ToString(bufferResult.error())));
		}
		BufferResource& buffer = bufferResult.value();

		std::expected<void*, vk::Result> mapResult = buffer.DeviceMemory.mapMemory(0, bufferSize);
		if (!mapResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DeviceMemoryMapError, ToString(mapResult.error())));
		}

		return UniformBufferResource{.Buffer = std::move(buffer), .BufferMapped = mapResult.value()};
	}

	CreationResult<vk::raii::DescriptorPool> CreateDescriptorPool(const vk::raii::Device& logicalDevice, const std::uint32_t descriptorCount)
	{
		vk::DescriptorPoolSize poolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = descriptorCount};
		const vk::DescriptorPoolCreateInfo poolInfo{
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, .maxSets = descriptorCount, .poolSizeCount = 1, .pPoolSizes = &poolSize};

		std::expected<vk::raii::DescriptorPool, vk::Result> poolCreationResult = logicalDevice.createDescriptorPool(poolInfo);
		if (!poolCreationResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DescriptorPoolCreateError, ToString(poolCreationResult.error())));
		}

		return std::move(poolCreationResult.value());
	}

	CreationResult<std::vector<vk::raii::DescriptorSet>> CreateDescriptorSets(const vk::raii::Device& logicalDevice,
																			  const vk::raii::DescriptorSetLayout& descriptorSetLayout,
																			  const vk::raii::DescriptorPool& descriptorPool,
																			  const std::uint32_t descriptorSetCount)
	{
		std::vector layouts(descriptorSetCount, *descriptorSetLayout);
		const vk::DescriptorSetAllocateInfo allocInfo{
			.descriptorPool = descriptorPool, .descriptorSetCount = static_cast<std::uint32_t>(layouts.size()), .pSetLayouts = layouts.data()};

		std::expected<std::vector<vk::raii::DescriptorSet>, vk::Result> descriptorSetsResult = logicalDevice.allocateDescriptorSets(allocInfo);
		if (!descriptorSetsResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DescriptorSetCreateError, ToString(descriptorSetsResult.error())));
		}

		return std::move(descriptorSetsResult.value());
	}

	CreationResult<std::vector<vk::raii::CommandBuffer>> AllocateCommandBuffers(const vk::raii::Device& logicalDevice,
																				const vk::raii::CommandPool& commandPool,
																				const std::uint32_t commandBufferCount)
	{
		const vk::CommandBufferAllocateInfo commandBufferAllocateInfo{
			.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = commandBufferCount};

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

	CreationResult<ImageResource> CreateTextureImage(const vk::raii::PhysicalDevice& physicalDevice,
													 const vk::raii::Device& logicalDevice,
													 const vk::raii::Queue& queue,
													 const vk::raii::CommandPool& commandPool,
													 const std::string_view textureFileName)
	{
		const std::expected<std::filesystem::path, PathError> executableDirectoryResult = GetExecutableDirectory();
		if (!executableDirectoryResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::TextureLoadError, ToString(executableDirectoryResult.error())));
		}

		const std::expected<std::vector<std::byte>, FileError> encodedBytesResult =
			ReadBinaryFile(executableDirectoryResult.value() / "Textures" / textureFileName);
		if (!encodedBytesResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::TextureLoadError, ToString(encodedBytesResult.error())));
		}

		const std::expected<Image, ImageError> decodedImageResult = DecodeImage(encodedBytesResult.value());
		if (!decodedImageResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::TextureLoadError, ToString(decodedImageResult.error())));
		}
		const Image& decodedImage = decodedImageResult.value();

		CreationResult<BufferResource> stagingBufferResult =
			CreateBufferResource(physicalDevice, logicalDevice, decodedImage.Pixels.size(), vk::BufferUsageFlagBits::eTransferSrc,
								 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		if (!stagingBufferResult)
		{
			return std::unexpected(stagingBufferResult.error());
		}
		const BufferResource& stagingBuffer = stagingBufferResult.value();

		if (CreationResult<void> uploadResult = UploadToDeviceMemory(stagingBuffer.DeviceMemory, decodedImage.Pixels); !uploadResult)
		{
			return std::unexpected(uploadResult.error());
		}

		CreationResult<ImageResource> imageResult =
			CreateImage(physicalDevice, logicalDevice, decodedImage.Width, decodedImage.Height, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
						vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
		if (!imageResult)
		{
			return std::unexpected(imageResult.error());
		}
		ImageResource& imageResource = imageResult.value();

		CreationResult<vk::raii::CommandBuffer> commandBufferResult = BeginSingleTimeCommands(logicalDevice, commandPool);
		if (!commandBufferResult)
		{
			return std::unexpected(commandBufferResult.error());
		}
		vk::raii::CommandBuffer& commandBuffer = commandBufferResult.value();

		TransitionImageLayout(commandBuffer, *imageResource.Image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
							  vk::AccessFlagBits2::eNone, vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eNone,
							  vk::PipelineStageFlagBits2::eTransfer);

		CopyBufferToImage(commandBuffer, stagingBuffer.Buffer, imageResource.Image, decodedImage.Width, decodedImage.Height);

		TransitionImageLayout(commandBuffer, *imageResource.Image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
							  vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eShaderSampledRead, vk::PipelineStageFlagBits2::eTransfer,
							  vk::PipelineStageFlagBits2::eFragmentShader);

		if (CreationResult<void> endResult = EndSingleTimeCommands(queue, std::move(commandBuffer)); !endResult)
		{
			return std::unexpected(endResult.error());
		}

		return std::move(imageResource);
	}

	CreationResult<ImageResource> CreateImage(const vk::raii::PhysicalDevice& physicalDevice,
											  const vk::raii::Device& logicalDevice,
											  const std::uint32_t width,
											  const std::uint32_t height,
											  const vk::Format format,
											  const vk::ImageTiling tiling,
											  const vk::ImageUsageFlags usage,
											  const vk::MemoryPropertyFlags properties)
	{
		const vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
											.format = format,
											.extent = {.width = width, .height = height, .depth = 1},
											.mipLevels = 1,
											.arrayLayers = 1,
											.samples = vk::SampleCountFlagBits::e1,
											.tiling = tiling,
											.usage = usage,
											.sharingMode = vk::SharingMode::eExclusive};
		std::expected<vk::raii::Image, vk::Result> imageResult = logicalDevice.createImage(imageInfo);
		if (!imageResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::ImageCreateError, ToString(imageResult.error())));
		}
		vk::raii::Image& image = imageResult.value();

		const vk::MemoryRequirements memoryRequirements = image.getMemoryRequirements();
		CreationResult<std::uint32_t> memoryTypeResult = FindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties);
		if (!memoryTypeResult)
		{
			return std::unexpected(memoryTypeResult.error());
		}

		const vk::MemoryAllocateInfo memoryAllocateInfo{.allocationSize = memoryRequirements.size, .memoryTypeIndex = memoryTypeResult.value()};
		std::expected<vk::raii::DeviceMemory, vk::Result> deviceMemoryResult = logicalDevice.allocateMemory(memoryAllocateInfo);
		if (!deviceMemoryResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DeviceMemoryAllocationError, ToString(deviceMemoryResult.error())));
		}
		vk::raii::DeviceMemory& deviceMemory = deviceMemoryResult.value();

		if (std::expected<void, vk::Result> bindResult = image.bindMemory(*deviceMemory, 0); !bindResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::DeviceMemoryBindError, ToString(bindResult.error())));
		}

		return ImageResource{.Image = std::move(image), .DeviceMemory = std::move(deviceMemory)};
	}

	CreationResult<vk::raii::CommandBuffer> BeginSingleTimeCommands(const vk::raii::Device& logicalDevice, const vk::raii::CommandPool& commandPool)
	{
		CreationResult<std::vector<vk::raii::CommandBuffer>> commandBufferResult = AllocateCommandBuffers(logicalDevice, commandPool, 1);
		if (!commandBufferResult)
		{
			return std::unexpected(commandBufferResult.error());
		}
		vk::raii::CommandBuffer& commandBuffer = commandBufferResult.value().front();

		constexpr vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
		if (std::expected<void, vk::Result> beginResult = commandBuffer.begin(beginInfo); !beginResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CommandBufferCopyError, ToString(beginResult.error())));
		}

		return std::move(commandBuffer);
	}

	CreationResult<void> EndSingleTimeCommands(const vk::raii::Queue& queue, vk::raii::CommandBuffer&& commandBuffer)
	{
		if (std::expected<void, vk::Result> endCommandBufferResult = commandBuffer.end(); !endCommandBufferResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CommandBufferCopyError, ToString(endCommandBufferResult.error())));
		}

		const vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};

		if (std::expected<void, vk::Result> submitResult = queue.submit(submitInfo, nullptr); !submitResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CommandBufferCopyError, ToString(submitResult.error())));
		}
		if (std::expected<void, vk::Result> waitResult = queue.waitIdle(); !waitResult)
		{
			return std::unexpected(RendererError(RendererCreationErrorKind::CommandBufferCopyError, ToString(waitResult.error())));
		}

		return {};
	}

	void TransitionImageLayout(const vk::raii::CommandBuffer& commandBuffer,
							   const vk::Image image,
							   const vk::ImageLayout oldLayout,
							   const vk::ImageLayout newLayout,
							   const vk::AccessFlags2 srcAccessMask,
							   const vk::AccessFlags2 dstAccessMask,
							   const vk::PipelineStageFlags2 srcStageMask,
							   const vk::PipelineStageFlags2 dstStageMask)
	{
		const vk::ImageMemoryBarrier2 barrier{
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
		const vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};

		commandBuffer.pipelineBarrier2(dependencyInfo);
	}

	void CopyBufferToImage(const vk::raii::CommandBuffer& commandBuffer,
						   const vk::raii::Buffer& buffer,
						   const vk::raii::Image& image,
						   const std::uint32_t width,
						   const std::uint32_t height)
	{
		const vk::BufferImageCopy region{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
			.imageOffset = {.x = 0, .y = 0, .z = 0},
			.imageExtent = {.width = width, .height = height, .depth = 1}};

		commandBuffer.copyBufferToImage(*buffer, *image, vk::ImageLayout::eTransferDstOptimal, region);
	}
}
