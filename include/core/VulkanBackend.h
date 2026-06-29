#pragma once
#include "core/RenderBackend.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

struct SDL_Window;

// Vulkan backend for FluentUI.
//
// Two modes, selected by what is passed as Init's `existingContext`:
//   - Standalone (existingContext == nullptr): the backend creates its own
//     VkInstance/device/swapchain/render pass and owns the whole frame
//     (acquire → record → submit → present). Good for pure-FluentUI apps.
//   - Shared (existingContext == VulkanSharedContext*): the backend reuses the
//     engine's device/queue/render pass and records UI draws onto the command
//     buffer supplied each frame via SetFrameCommandBuffer(). It performs NO
//     submit/present — the engine drives the frame.
//
// Shaders are precompiled SPIR-V embedded in EmbeddedShadersVulkan.h.
// Y-flip matches the GL ortho matrix via a negative-height viewport (requires
// Vulkan 1.1 / VK_KHR_maintenance1).

namespace FluentUI {

class VulkanBackend : public RenderBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend() override;

    bool Init(void* windowHandle, void* existingContext = nullptr) override;
    void Shutdown() override;
    void BeginFrame(const Color& clearColor) override;
    void EndFrame() override;
    void SetViewport(int width, int height) override;

    void SetFrameCommandBuffer(void* cmdBuffer) override;

    void PushClipRect(int x, int y, int width, int height) override;
    void PopClipRect() override;

    void* CreateTexture(int width, int height, const void* data, bool alphaOnly = false) override;
    void UpdateTexture(void* textureHandle, int x, int y, int width, int height, const void* data) override;
    void DeleteTexture(void* textureHandle) override;
    void* RegisterExternalTexture(void* nativeView, void* sampler, int layout) override;

    void DrawBatch(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                   const unsigned int* indices, size_t indexCount,
                   void* textureHandle, const float* projectionMatrix,
                   const Color& textColor = {1, 1, 1, 1}) override;
    void DrawLines(const RenderVertex* vertices, size_t vertexCount,
                   float width, const float* projectionMatrix) override;
    void DrawSDFInstances(const SDFInstance* instances, size_t count,
                          const float* projectionMatrix,
                          const float* revealCursor = nullptr) override;

private:
    // ── Configuration ──────────────────────────────────────────────────
    static constexpr int kRingSize = 3;          // dynamic-buffer ring slots
    static constexpr int kFramesInFlight = 2;    // standalone CPU/GPU overlap
    static constexpr VkDeviceSize kVertexBytesPerSlot = 4u * 1024 * 1024; // 4 MB
    static constexpr VkDeviceSize kIndexBytesPerSlot  = 1u * 1024 * 1024; // 1 MB

    // 96 bytes — fits the 128-byte guaranteed push-constant range.
    // reveal[3] = {cursorX, cursorY, revealRadius} (logical px) for the SDF reveal
    // highlight (brief 04); scalar-packed in the shader to match this C layout.
    struct PushConstants {
        float projection[16];
        float textColor[4];
        float pxRange;
        float reveal[3];
    };

    // ── Mode / shared state ────────────────────────────────────────────
    bool ownsDevice = false;       // standalone created instance+device
    bool sharedMode = false;       // record onto an external command buffer
    SDL_Window* window = nullptr;

    VkInstance       instance       = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    uint32_t         queueFamilyIndex = 0;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkFormat         colorFormat    = VK_FORMAT_B8G8R8A8_UNORM;
    bool             wideLinesSupported = false;
    // True when the color attachment is an sRGB format (the engine's render pass
    // in shared mode usually is). The GPU then applies linear→sRGB on write, so
    // we pre-convert our already-sRGB-authored colors to linear before upload to
    // avoid a double gamma encode (washed-out colors, lost soft shadows).
    bool             srgbTarget = false;
    // Reusable scratch for the sRGB-target color pre-conversion (avoids a
    // per-draw heap allocation).
    std::vector<RenderVertex> scratchVerts;
    // One-shot guard so the "no external command buffer" warning isn't spammed
    // every frame when the host forgets SetFrameCommandBuffer() in shared mode.
    bool warnedNoExternalCmd = false;

    // ── Standalone swapchain ───────────────────────────────────────────
    VkSurfaceKHR              surface    = VK_NULL_HANDLE;
    VkSwapchainKHR            swapchain  = VK_NULL_HANDLE;
    VkExtent2D                swapExtent{};
    std::vector<VkImage>      swapImages;
    std::vector<VkImageView>  swapImageViews;
    std::vector<VkFramebuffer> framebuffers;
    uint32_t                  currentImageIndex = 0;
    bool                      swapchainDirty = false;

    // Render pass: owned in standalone, borrowed from the engine in shared mode.
    // When useDynamicRendering is true it stays VK_NULL_HANDLE and pipelines are
    // built with VkPipelineRenderingCreateInfo instead.
    VkRenderPass renderPass = VK_NULL_HANDLE;
    bool         ownsRenderPass = false;

    // Dynamic rendering (engine uses vkCmdBeginRendering, no VkRenderPass).
    bool     useDynamicRendering = false;
    VkFormat depthFormat   = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;

    // ── Pipelines ──────────────────────────────────────────────────────
    VkShaderModule vertModule  = VK_NULL_HANDLE;
    VkShaderModule basicFrag   = VK_NULL_HANDLE;
    VkShaderModule textFrag    = VK_NULL_HANDLE;
    VkShaderModule msdfFrag    = VK_NULL_HANDLE;
    VkShaderModule imageFrag   = VK_NULL_HANDLE;
    VkShaderModule sdfVertModule = VK_NULL_HANDLE; // SDF needs its own instanced vertex input
    VkShaderModule sdfFrag       = VK_NULL_HANDLE;

    VkDescriptorSetLayout texSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout layoutNoTex = VK_NULL_HANDLE; // Basic, Lines
    VkPipelineLayout layoutTex   = VK_NULL_HANDLE; // Text, MSDF, Image

    VkPipeline pipeBasic = VK_NULL_HANDLE;
    VkPipeline pipeText  = VK_NULL_HANDLE;
    VkPipeline pipeMSDF  = VK_NULL_HANDLE;
    VkPipeline pipeImage = VK_NULL_HANDLE;
    VkPipeline pipeLines = VK_NULL_HANDLE;
    VkPipeline pipeSDF   = VK_NULL_HANDLE; // instanced SDF rounded rect (brief 01)

    // Static unit-quad geometry for the SDF pipeline (binding 0 + index buffer).
    VkBuffer       sdfQuadVbo    = VK_NULL_HANDLE;
    VkDeviceMemory sdfQuadVboMem = VK_NULL_HANDLE;
    VkBuffer       sdfQuadIbo    = VK_NULL_HANDLE;
    VkDeviceMemory sdfQuadIboMem = VK_NULL_HANDLE;
    // Reusable scratch for sRGB-target linearization of instance colors.
    std::vector<SDFInstance> scratchInstances;

    // ── Dynamic vertex/index buffers (ring) ────────────────────────────
    struct DynBuffer {
        VkBuffer       vbo = VK_NULL_HANDLE;
        VkBuffer       ebo = VK_NULL_HANDLE;
        VkDeviceMemory vboMem = VK_NULL_HANDLE;
        VkDeviceMemory eboMem = VK_NULL_HANDLE;
        void*          vboMapped = nullptr;
        void*          eboMapped = nullptr;
        VkDeviceSize   vboOffset = 0;
        VkDeviceSize   eboOffset = 0;
    };
    DynBuffer ring[kRingSize];
    int currentRing = 0;
    bool ringOverflowWarned = false;

    // ── Standalone per-frame sync ──────────────────────────────────────
    struct FrameSync {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkFence     inFlight = VK_NULL_HANDLE;
    };
    FrameSync frames[kFramesInFlight];
    std::vector<VkSemaphore> renderFinished; // one per swapchain image
    int currentFrame = 0;

    VkCommandPool commandPool = VK_NULL_HANDLE; // standalone frame cmds
    VkCommandPool uploadPool  = VK_NULL_HANDLE; // texture staging (both modes)

    // The command buffer currently being recorded (standalone: frame cmd;
    // shared: the engine-supplied buffer).
    VkCommandBuffer currentCmd = VK_NULL_HANDLE;
    VkCommandBuffer externalCmd = VK_NULL_HANDLE;

    // ── Textures ───────────────────────────────────────────────────────
    struct VulkanTexture {
        VkImage         image = VK_NULL_HANDLE;
        VkDeviceMemory  memory = VK_NULL_HANDLE;
        VkImageView     view = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE; // pool the set came from (for vkFreeDescriptorSets)
        int width = 0, height = 0;
        bool alphaOnly = false;
        // External textures wrap a host-owned image/view (e.g. an engine viewport).
        // The backend owns only the descriptor; it must NOT destroy image/memory/view.
        bool external = false;
    };
    VkSampler sampler = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> descriptorPools;
    std::vector<VulkanTexture*> textures;

    // ── Viewport / clip ────────────────────────────────────────────────
    VkExtent2D logicalViewport{800, 600};
    struct ClipRect { int x, y, w, h; };
    std::vector<ClipRect> clipStack;

    // ── Internal helpers ───────────────────────────────────────────────
    bool CreateInstanceAndDevice();             // standalone only
    bool PickPhysicalDevice();                  // standalone only
    bool CreateSwapchain();                     // standalone only
    void DestroySwapchain();
    void RecreateSwapchain();
    bool CreateStandaloneRenderPass();
    bool CreateSyncAndCommands();               // standalone only
    bool CreateShaderModules();
    bool CreatePipelines();
    bool CreateDynamicBuffers();
    bool CreateSamplerAndDescriptorInfra();

    VkShaderModule MakeShaderModule(const uint32_t* code, size_t bytes);
    VkPipeline MakePipeline(VkShaderModule frag, VkPipelineLayout layout,
                            VkPrimitiveTopology topology, bool dynamicLineWidth);
    VkPipeline MakeSDFPipeline(); // dedicated instanced vertex input (brief 01)
    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;
    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem);
    VkDescriptorSet AllocateTextureDescriptor(VkDescriptorPool& outPool);

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer cmd);
    void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout);

    void SetFullViewportAndScissor();
    void ApplyScissor();
    void RecordDraw(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                    const unsigned int* indices, size_t indexCount,
                    void* textureHandle, const float* projectionMatrix,
                    const Color& textColor, bool isLines, float lineWidth);
};

} // namespace FluentUI
