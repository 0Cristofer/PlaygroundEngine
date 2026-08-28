export module PlaygroundEngine.Renderer.Vulkan;

import PlaygroundEngine.WindowServer;

export import :VulkanTypes;

// An interface partition has to be reachable from the primary interface, and GCC rejects a plain
// 'import' of one here. Re-exporting publishes nothing, since :VulkanUtils exports no declaration.

export import :VulkanUtils;

import vulkan;
import imgui;
import std;
import PlaygroundEngine.Math;
import PlaygroundEngine.Renderer.Vertex;
import PlaygroundEngine.Renderer.Frame;
import PlaygroundEngine.Renderer.Mesh;
import PlaygroundEngine.Renderer.View;

namespace PgE
{
	export class RendererVulkan
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<RendererVulkan>, RendererError<RendererCreationErrorKind>> Create(
			const RendererSpecification& specification, const WindowServer& windowServer, const Window& window);

		void Teardown() const;

		/// Uploads a model file from the Models folder beside the executable and names the result.
		/// Nothing is de-duplicated here: a caller that may ask twice keeps its own path-to-handle map.
		[[nodiscard]] CreationResult<MeshHandle> AcquireMesh(std::string_view modelFileName);

		/// frame is what the simulation extracted for this step; the aspect ratio comes from the swap chain.
		/// debugUiDrawData is ImGui's output for this frame, drawn in an overlay pass after the scene.
		/// Null draws no overlay.
		std::expected<void, RendererError<RendererRenderErrorKind>> DrawFrame(const ExtractedFrame& frame,
																			  FramebufferSize framebufferSize,
																			  ImDrawData* debugUiDrawData = nullptr);

		void NotifyFramebufferResized();

		/// Runs the overlay backend's per-frame step. Must precede the ImGui frame it belongs to, so
		/// the loop calls this before DebugUi::BeginFrame. A no-op when the overlay is off.
		void BeginDebugUiFrame() const;

		/// False when the overlay was not requested, or was requested and failed to come up. Callers
		/// need not consult it before passing draw data: an overlay that is off simply draws nothing.
		[[nodiscard]] bool IsDebugUiOverlayEnabled() const
		{
			return _debugUiOverlayEnabled;
		}

		/// Queues a capture of the next presented frame, writing it to path as a PNG.
		/// Fire and forget. The frame is not failed if the capture is.
		void RequestCapture(std::filesystem::path path);

	private:
		struct GpuMesh
		{
			BufferResource VertexBuffer;
			BufferResource IndexBuffer;
			std::uint32_t IndexCount;
		};

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
			  _textureImageResource(std::move(textureImageResource)), _textureImageView(std::move(textureImageView)),
			  _textureSampler(std::move(textureSampler)), _sampleCount(sampleCount),
			  _multisampleColorImageResource(std::move(multisampleColorImageResource)),
			  _multisampleColorImageView(std::move(multisampleColorImageView)), _depthFormat(depthFormat),
			  _depthImageResource(std::move(depthImageResource)), _depthImageView(std::move(depthImageView)),
			  _uniformBufferResources(std::move(uniformBufferResources)), _descriptorPool(std::move(descriptorPool)),
			  _descriptorSets(std::move(descriptorSets)), _commandBuffers(std::move(commandBuffer)),
			  _presentCompleteSemaphores(std::move(presentCompleteSemaphores)), _renderFinishedSemaphores(std::move(renderFinishedSemaphores)),
			  _inFlightFences(std::move(inFlightFences))
		{}

		/// Attaches ImGui's Vulkan backend to the current ImGui context. Separate from the constructor
		/// because it is the one piece of setup that reads state owned outside the renderer.
		void InitializeDebugUiBackend(std::uint32_t queueFamilyIndex);

		std::expected<void, RendererError<RendererRenderErrorKind>> RecordCommandBuffer(const ExtractedFrame& frame,
																						std::uint32_t imageIndex,
																						ImDrawData* debugUiDrawData) const;
		void RecordDebugUiOverlay(std::uint32_t imageIndex, ImDrawData* debugUiDrawData) const;
		CreationResult<void> CaptureSwapChainImage(std::uint32_t imageIndex, const std::filesystem::path& path) const;
		std::expected<void, RendererError<RendererRenderErrorKind>> RecreateSwapChain(FramebufferSize framebufferSize);
		void UpdateUniformBuffer(std::uint32_t frameIndex, const ExtractedView& view) const;

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

		// Indexed by MeshHandle::Index minus one, so a default-constructed handle names nothing.
		std::vector<GpuMesh> _meshes;

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
		bool _debugUiOverlayEnabled = false;
		std::optional<std::filesystem::path> _pendingCapturePath;
	};
}
