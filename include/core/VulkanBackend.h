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

    // --- Render targets / SaveState (brief 05) ---
    void* CreateRenderTarget(int width, int height) override;
    void  SetRenderTarget(void* target) override;
    void* GetRenderTargetTexture(void* target) override;
    void  DeleteRenderTarget(void* target) override;
    void  CopyTexture(void* src, void* dst, int width, int height) override;
    void  SaveState() override;
    void  RestoreState() override;

private:
    // ── Configuration ──────────────────────────────────────────────────
    static constexpr int kRingSize = 3;          // dynamic-buffer ring slots
    static constexpr int kFramesInFlight = 2;    // standalone CPU/GPU overlap
    static constexpr VkDeviceSize kVertexBytesPerSlot = 4u * 1024 * 1024; // 4 MB
    static constexpr VkDeviceSize kIndexBytesPerSlot  = 1u * 1024 * 1024; // 1 MB

    // 84 bytes — fits the 128-byte guaranteed push-constant range.
    struct PushConstants {
        float projection[16];
        float textColor[4];
        float pxRange;
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

    VkDescriptorSetLayout texSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout layoutNoTex = VK_NULL_HANDLE; // Basic, Lines
    VkPipelineLayout layoutTex   = VK_NULL_HANDLE; // Text, MSDF, Image

    VkPipeline pipeBasic = VK_NULL_HANDLE;
    VkPipeline pipeText  = VK_NULL_HANDLE;
    VkPipeline pipeMSDF  = VK_NULL_HANDLE;
    VkPipeline pipeImage = VK_NULL_HANDLE;
    VkPipeline pipeLines = VK_NULL_HANDLE;

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

    // ── Render targets (brief 05) ──────────────────────────────────────
    // Offscreen color target usable as a sampled texture by the Image shader
    // (and by brief 06's blur passes). Color image with usage
    // COLOR_ATTACHMENT | SAMPLED | TRANSFER_SRC | TRANSFER_DST, DEVICE_LOCAL.
    struct VkRenderTarget {
        VkImage        image       = VK_NULL_HANDLE;
        VkDeviceMemory memory      = VK_NULL_HANDLE;
        VkImageView    view        = VK_NULL_HANDLE;
        VkFramebuffer  framebuffer = VK_NULL_HANDLE; // classic render pass only
        VkRenderPass   pass        = VK_NULL_HANDLE; // offscreen pass (classic); null w/ dynamic rendering
        VulkanTexture* sampleTex   = nullptr;        // descriptor wrapper to draw it via Image
        VkImageLayout  layout      = VK_IMAGE_LAYOUT_UNDEFINED;
        int width = 0, height = 0;
    };
    std::vector<VkRenderTarget*> renderTargets;
    VkRenderTarget* currentRT = nullptr;             // nullptr = swapchain/engine target

    // Offscreen recording. In BOTH modes the main target's render pass stays
    // active across UI draws (standalone begins the swapchain pass in BeginFrame;
    // in shared mode the engine is already inside its pass). A nested render pass
    // on that command buffer is illegal, so offscreen RT passes are recorded on a
    // private, transient command buffer and submitted (fence-synced) when we
    // return to the main target. `mainCmd` holds the paused main buffer.
    VkCommandBuffer offscreenCmd = VK_NULL_HANDLE;
    VkCommandBuffer mainCmd      = VK_NULL_HANDLE;

    // SaveState/RestoreState stack (active RT + clip stack). Vulkan has no global
    // mutable state like GL; what matters across nested passes is the active
    // target and scissor/clip.
    struct SavedState {
        VkRenderTarget*       rt = nullptr;
        std::vector<ClipRect> clipStack;
    };
    std::vector<SavedState> stateStack;

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

    // Render-target helpers (brief 05).
    bool CreateOffscreenRenderPass(VkRenderTarget* rt);
    VulkanTexture* CreateRTSampleTexture(VkRenderTarget* rt);
    void BeginOffscreenRecording();   // lazily begin offscreenCmd, switch currentCmd
    void BeginRTPass(VkRenderTarget* rt);
    void EndRTPass();                 // end the active RT pass (classic/dynamic)
    void FlushOffscreen();            // submit offscreenCmd, fence-wait, restore mainCmd
    void DestroyRenderTarget(VkRenderTarget* rt); // assumes device idle
    void RecordDraw(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                    const unsigned int* indices, size_t indexCount,
                    void* textureHandle, const float* projectionMatrix,
                    const Color& textColor, bool isLines, float lineWidth);
};

} // namespace FluentUI
