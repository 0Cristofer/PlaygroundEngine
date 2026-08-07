module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module PlaygroundEngine.Renderer.Vulkan;

import PlaygroundEngine.WindowServer;

export import :VulkanTypes;

// An interface partition has to be reachable from the primary interface, and GCC rejects a plain
// 'import' of one here. Re-exporting publishes nothing, since :VulkanUtils exports no declaration.

export import :VulkanUtils;

import vulkan;
import std;
import PlaygroundEngine.Renderer.Vertex;

namespace PgE
{
	export class RendererVulkan
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<RendererVulkan>, RendererError<RendererCreationErrorKind>> Create(
			const RendererSpecification& specification, const WindowServer& windowServer, const Window& window);

		void Teardown() const;

		std::expected<void, RendererError<RendererRenderErrorKind>> DrawFrame(const PlatformEventRecord& platformEventRecord,
																			  FramebufferSize framebufferSize);

		void NotifyFramebufferResized();

		/// Queues a capture of the next presented frame, writing it to path as a PNG.
		/// Fire and forget. The frame is not failed if the capture is.
		void RequestCapture(std::filesystem::path path);

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
					   bool swapChainSupportsTransferSource,
					   std::vector<vk::raii::ImageView> swapChainImageViews,
					   vk::raii::DescriptorSetLayout descriptorSetLayout,
					   vk::raii::PipelineLayout pipelineLayout,
					   vk::raii::Pipeline graphicsPipeline,
					   vk::raii::CommandPool commandPool,
					   BufferResource vertexBufferResource,
					   BufferResource indexBufferResource,
					   std::uint32_t indexCount,
					   ImageResource textureImageResource,
					   vk::raii::ImageView textureImageView,
					   vk::raii::Sampler textureSampler,
					   vk::SampleCountFlagBits sampleCount,
					   ImageResource multisampleColorImageResource,
					   vk::raii::ImageView multisampleColorImageView,
					   vk::Format depthFormat,
					   ImageResource depthImageResource,
					   vk::raii::ImageView depthImageView,
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
			  _swapChainSupportsTransferSource(swapChainSupportsTransferSource), _swapChainImageViews(std::move(swapChainImageViews)),
			  _descriptorSetLayout(std::move(descriptorSetLayout)), _pipelineLayout(std::move(pipelineLayout)),
			  _graphicsPipeline(std::move(graphicsPipeline)), _commandPool(std::move(commandPool)),
			  _vertexBufferResource(std::move(vertexBufferResource)), _indexBufferResource(std::move(indexBufferResource)), _indexCount(indexCount),
			  _textureImageResource(std::move(textureImageResource)), _textureImageView(std::move(textureImageView)),
			  _textureSampler(std::move(textureSampler)), _sampleCount(sampleCount),
			  _multisampleColorImageResource(std::move(multisampleColorImageResource)),
			  _multisampleColorImageView(std::move(multisampleColorImageView)), _depthFormat(depthFormat),
			  _depthImageResource(std::move(depthImageResource)), _depthImageView(std::move(depthImageView)),
			  _uniformBufferResources(std::move(uniformBufferResources)), _descriptorPool(std::move(descriptorPool)),
			  _descriptorSets(std::move(descriptorSets)), _commandBuffers(std::move(commandBuffer)),
			  _presentCompleteSemaphores(std::move(presentCompleteSemaphores)), _renderFinishedSemaphores(std::move(renderFinishedSemaphores)),
			  _inFlightFences(std::move(inFlightFences))
		{
			// The model is authored Z-up, so it is rotated into the engine's Y-up world once, here.
			// Everything downstream (camera, movement) is plain Y-up.

			const glm::mat4 zUpToYUp = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

			_ubo.Model = glm::rotate(zUpToYUp, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			_ubo.Proj = glm::perspective(glm::radians(45.0f),
										 static_cast<float>(_swapChainExtent.width) / static_cast<float>(_swapChainExtent.height), 0.1f, 10.0f);

			_ubo.Proj[1][1] *= -1;

			RebuildViewMatrix();
		}

		std::expected<void, RendererError<RendererRenderErrorKind>> RecordCommandBuffer(std::uint32_t imageIndex) const;
		CreationResult<void> CaptureSwapChainImage(std::uint32_t imageIndex, const std::filesystem::path& path) const;
		std::expected<void, RendererError<RendererRenderErrorKind>> RecreateSwapChain(FramebufferSize framebufferSize);
		void UpdateUniformBuffer(std::uint32_t frameIndex) const;

		/// Throwaway free-look camera, here only to give the platform event path something real to drive.
		/// WASD moves relative to where the camera looks, arrow keys turn it. Delete along with the demo.

		struct CameraInputState
		{
			bool MoveForward = false;
			bool MoveBackward = false;
			bool StrafeLeft = false;
			bool StrafeRight = false;
			bool TurnLeft = false;
			bool TurnRight = false;
			bool LookUp = false;
			bool LookDown = false;
		};

		static CameraInputState ReadCameraInput(const PlatformEventRecord& platformEventRecord, CameraInputState previousState);
		void MoveCamera(CameraInputState cameraInput, float deltaTimeSeconds);
		void RebuildViewMatrix();
		glm::vec3 GetCameraForward() const;

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
		bool _swapChainSupportsTransferSource;
		std::vector<vk::raii::ImageView> _swapChainImageViews;

		vk::raii::DescriptorSetLayout _descriptorSetLayout;
		vk::raii::PipelineLayout _pipelineLayout;
		vk::raii::Pipeline _graphicsPipeline;
		vk::raii::CommandPool _commandPool;

		BufferResource _vertexBufferResource;
		BufferResource _indexBufferResource;
		std::uint32_t _indexCount;

		ImageResource _textureImageResource;
		vk::raii::ImageView _textureImageView;
		vk::raii::Sampler _textureSampler;

		vk::SampleCountFlagBits _sampleCount;
		ImageResource _multisampleColorImageResource;
		vk::raii::ImageView _multisampleColorImageView;

		vk::Format _depthFormat;
		ImageResource _depthImageResource;
		vk::raii::ImageView _depthImageView;

		std::vector<UniformBufferResource> _uniformBufferResources;
		vk::raii::DescriptorPool _descriptorPool;
		std::vector<vk::raii::DescriptorSet> _descriptorSets;

		std::vector<vk::raii::CommandBuffer> _commandBuffers;

		std::vector<vk::raii::Semaphore> _presentCompleteSemaphores;
		std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;
		std::vector<vk::raii::Fence> _inFlightFences;

		std::uint32_t _frameIndex = 0;
		bool _framebufferResized = false;
		std::optional<std::filesystem::path> _pendingCapturePath;

		UniformBufferObject _ubo;

		// Yaw and pitch are the (2,2,2)-looking-at-the-origin framing the demo started from,
		// expressed in the angles GetCameraForward() consumes.

		glm::vec3 _cameraPosition{2.0f, 2.0f, 2.0f};
		float _cameraYaw = glm::radians(-45.0f);
		float _cameraPitch = glm::radians(-35.264f);
		CameraInputState _cameraInput;
		std::chrono::steady_clock::time_point _previousFrameTime = std::chrono::steady_clock::now();
	};
}
