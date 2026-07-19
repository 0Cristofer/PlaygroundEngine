module;

#include "PlaygroundEngine/Log.h"

#include <vulkan/vulkan.h>

module PlaygroundEngine.RendererVulkan;

import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;

import vulkan;

namespace PgE
{
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

	std::expected<std::unique_ptr<RendererVulkan>, RendererError> RendererVulkan::Create(const RendererSpecification& specification,
																						 const Window& window)
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
			switch (requiredWindowExtensionsResult.error())
			{
			default:
				return std::unexpected(RendererError(RendererErrorKind::UnableToRequestExtensions, ToString(requiredWindowExtensionsResult.error())));
			}
		}
		std::span<const char* const> requiredWindowExtensions = requiredWindowExtensionsResult.value();

		std::expected<std::vector<vk::ExtensionProperties>, vk::Result> availableExtensionsResult = context.enumerateInstanceExtensionProperties();
		if (!availableExtensionsResult)
		{
			switch (availableExtensionsResult.error())
			{
			default:
				return std::unexpected(RendererError(RendererErrorKind::ExtensionsEnumerationError, ToString(availableExtensionsResult.error())));
			}
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
				return std::unexpected(RendererError(RendererErrorKind::ExtensionUnavailable, requiredExtension));
			}
		}

		std::vector<const char*> requiredValidationLayers;
#if defined(PGE_DEV)
		requiredValidationLayers = {"VK_LAYER_KHRONOS_validation"};
#endif

		std::expected<std::vector<vk::LayerProperties>, vk::Result> availableValidationLayersResult = context.enumerateInstanceLayerProperties();
		if (!availableValidationLayersResult)
		{
			switch (availableValidationLayersResult.error())
			{
			default:
				return std::unexpected(
					RendererError(RendererErrorKind::ValidationLayersEnumerationError, ToString(availableValidationLayersResult.error())));
			}
		}
		const std::vector<vk::LayerProperties>& availableValidationLayers = availableValidationLayersResult.value();

		for (const char* const& requiredValidationLayer : requiredValidationLayers)
		{
			if (std::ranges::none_of(availableValidationLayers, [&requiredValidationLayer](const vk::LayerProperties& providedLayer) {
					return requiredValidationLayer == std::string_view(providedLayer.layerName);
				}))
			{
				return std::unexpected(RendererError(RendererErrorKind::ValidationLayerUnavailable, requiredValidationLayer));
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
			switch (instanceResult.error())
			{
			default:
				return std::unexpected(RendererError(RendererErrorKind::InstanceCreationError, ToString(instanceResult.error())));
			}
		}
		vk::raii::Instance& instance = instanceResult.value();

#if defined(PGE_DEV)
		std::expected<vk::raii::DebugUtilsMessengerEXT, vk::Result> debugMessengerResult =
			instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfo);
		if (!debugMessengerResult)
		{
			switch (debugMessengerResult.error())
			{
			default:
				return std::unexpected(RendererError(RendererErrorKind::DebugMessengerCreationError, ToString(debugMessengerResult.error())));
			}
		}
		vk::raii::DebugUtilsMessengerEXT& debugMessenger = debugMessengerResult.value();
#endif

		std::expected<VkSurfaceKHR, VulkanWindowError> surfaceCreationResult = window.CreateVulkanSurface(*instance);
		if (!surfaceCreationResult)
		{
			switch (surfaceCreationResult.error())
			{
			default:
				return std::unexpected(RendererError(RendererErrorKind::UnableToCreateWindowSurface, ToString(surfaceCreationResult.error())));
			}
		}
		auto surface = vk::raii::SurfaceKHR(instance, surfaceCreationResult.value());

		// Physical device selection

		auto physicalDevicesResult = instance.enumeratePhysicalDevices();
		if (!physicalDevicesResult)
		{
			switch (physicalDevicesResult.error())
			{
			default:
				return std::unexpected(RendererError(RendererErrorKind::PhysicalDevicesEnumerationError, ToString(physicalDevicesResult.error())));
			}
		}
		std::vector<vk::raii::PhysicalDevice>& physicalDevices = physicalDevicesResult.value();

		if (physicalDevices.empty())
		{
			return std::unexpected(RendererError(RendererErrorKind::NoPhysicalDevices));
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
			return std::unexpected(RendererError(RendererErrorKind::NoSuitablePhysicalDevices));
		}

		// Logical device setup

		vk::raii::PhysicalDevice& physicalDevice = suitableDevices.front();

		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
		uint32_t queueIndex = ~0;
		for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
		{
			if (queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
			{
				// found a queue family that supports both graphics and present
				queueIndex = qfpIndex;
				break;
			}
		}
		if (queueIndex == static_cast<uint32_t>(~0))
		{
			return std::unexpected(RendererError(RendererErrorKind::SuitableQueueNotFound));
		}

		float queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};

		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features,
						   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
			featureChain = {
				{},								// vk::PhysicalDeviceFeatures2 (empty for now)
				{.shaderDrawParameters = true}, // Enable shader draw parameters from Vulkan 1.1
				{.dynamicRendering = true},		// Enable dynamic rendering from Vulkan 1.3
				{.extendedDynamicState = true}	// Enable extended dynamic state from the extension
			};
		std::vector requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

		vk::DeviceCreateInfo deviceCreateInfo{.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
											  .queueCreateInfoCount = 1,
											  .pQueueCreateInfos = &deviceQueueCreateInfo,
											  .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
											  .ppEnabledExtensionNames = requiredDeviceExtension.data()};

		std::expected<vk::raii::Device, vk::Result> deviceResult = physicalDevice.createDevice(deviceCreateInfo);
		if (!deviceResult)
		{
			switch (deviceResult.error())
			{
			default:
				return std::unexpected(RendererError(RendererErrorKind::LogicalDeviceCreationError, ToString(deviceResult.error())));
			}
		}

		vk::raii::Device& logicalDevice = deviceResult.value();
		vk::raii::Queue queue = logicalDevice.getQueue(queueIndex, 0);

#if defined(PGE_DEV)
		return std::unique_ptr<RendererVulkan>(new RendererVulkan(std::move(context), std::move(instance), std::move(debugMessenger),
																  std::move(surface), std::move(physicalDevice), std::move(logicalDevice),
																  std::move(queue)));
#else
		return std::unique_ptr<RendererVulkan>(new RendererVulkan(std::move(context), std::move(instance), nullptr,
																std::move(surface), std::move(physicalDevice), std::move(logicalDevice),
																std::move(queue)));
#endif
	}
}
