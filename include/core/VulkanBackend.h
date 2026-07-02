#pragma once
#include "core/RenderBackend.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

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

    // brief 08 Part B: publish this backend's device-owner handles so a secondary
    // window can create a swapchain over the SAME device. Fills instance/
    // physicalDevice/device/graphicsQueue/queueFamilyIndex/colorFormat/sampleCount
    // and sets out->ownSwapchain = true (pass it straight as existingContext to a
    // new backend's Init). Returns false if this backend has no device yet.
    bool GetSharedContext(VulkanSharedContext* out) const;

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

    // --- Acrylic / Mica backdrop (brief 06) ---
    // Intentionally NOT supported yet → the Renderer uses DrawRectAcrylicFallback
    // (flat tinted fills) on Vulkan. Real GPU blur needs to capture the backdrop,
    // which is fundamentally incompatible with the brief-05 frame model: the main
    // color pass is active across ALL UI draws (standalone begins the swapchain pass
    // in BeginFrame; shared mode is inside the engine's pass), and you cannot sample
    // an attachment while its pass is recording/active. The validated path for a
    // follow-up is: copy the swapchain image to a persistent half-res "backdrop"
    // image at EndFrame (after vkCmdEndRenderPass, before present, via a TRANSFER_SRC
    // barrier + blit), then at the NEXT BeginFrame blur that stable image (dual
    // Kawase) on a transient, fence-synced command buffer into offscreen RTs, and
    // composite during the main pass with the (already-compiled) blur/composite
    // pipelines — ShadersVK::Blur_Vert / KawaseDown_Frag / KawaseUp_Frag /
    // AcrylicComposite_Vert / AcrylicComposite_Frag. This is a 1-frame-stale backdrop
    // (fine for Acrylic/Mica) and must be checked with the validation layers, so it is
    // deliberately deferred rather than shipped unvalidated.
    // #5: real Acrylic/Mica via content-behind capture. Whether it's active is a
    // RUNTIME capability (acrylicReady) — reported dynamically by Capabilities()
    // below, so SupportsAcrylic()/Supports(RenderCap::Acrylic) reflect the real state.
    // --- Capabilities (brief 24) ---
    uint32_t Capabilities() const override;
    void DrawAcrylicPanel(const AcrylicParams& params, const float* projectionMatrix) override;

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
    // brief 08 Part B: secondary window on a shared device — owns its surface/
    // swapchain/render pass/sync and presents, but NOT the device/instance.
    bool ownSwapchainOnSharedDevice = false;
    // gap #4: true when this backend created its own shader modules / pipeline layouts;
    // false when a secondary window ADOPTED them from the owner (must NOT destroy them).
    bool ownsShaderResources = true;
    // gap #4 Phase 2: false when a secondary window ADOPTED the owner's UI pipelines
    // (render pass was compatible) → must NOT destroy them.
    bool ownsPipelines = true;
    // gap #4 Phase 3: acrylic shaders/layouts adopted from the owner (pipelines + RTs
    // stay per-window). resourceOwner_ = the owner backend used to adopt (null unless a
    // secondary shared-device window).
    bool ownsAcrylicShaderResources = true;
    VulkanBackend* resourceOwner_ = nullptr;
    void* window = nullptr;  // native window handle (opaque; cast in the .cpp)

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

    // ── Acrylic/Mica (#5) — backdrop capture + dual-Kawase + composite ──────
    // 1-frame-stale backdrop: blit the swapchain image to backdropRT at EndFrame,
    // blur it at the next BeginFrame (phase 2), composite during the main pass.
    VkShaderModule acrylicCompVert = VK_NULL_HANDLE;
    VkShaderModule acrylicCompFrag = VK_NULL_HANDLE;
    VkShaderModule blurVert        = VK_NULL_HANDLE; // phase 2 (fullscreen triangle)
    VkShaderModule kawaseDownFrag  = VK_NULL_HANDLE; // phase 2
    VkShaderModule kawaseUpFrag    = VK_NULL_HANDLE; // phase 2
    VkDescriptorSetLayout acrylicSetLayout = VK_NULL_HANDLE; // 2 samplers: blur + noise
    VkPipelineLayout      acrylicLayout    = VK_NULL_HANDLE; // 124-byte push range
    VkPipeline            pipeAcrylicComposite = VK_NULL_HANDLE;
    VkPipeline            pipeKawaseDown = VK_NULL_HANDLE; // phase 2
    VkPipeline            pipeKawaseUp   = VK_NULL_HANDLE; // phase 2
    VkPipelineLayout      kawaseLayout   = VK_NULL_HANDLE; // 1 sampler + 8-byte push (phase 2)
    bool                  blurReady      = false;          // blurResult holds a valid blur this frame
    // Correct capture (content-BEHIND): we break the main pass at the first acrylic
    // panel, blit the in-progress swapchain (= what's behind the panel), blur it, then
    // resume the main pass with loadOp=LOAD to composite on top. acrylicLoadPass is that
    // resume pass; backdropCapturedThisFrame guards so we only do it once per frame.
    VkRenderPass acrylicLoadPass = VK_NULL_HANDLE;
    bool         backdropCapturedThisFrame = false;
    // backdropRT / blurResult / blurChain are declared after the VkRenderTarget struct (below).
    VkDescriptorSet       acrylicDescriptor     = VK_NULL_HANDLE; // binding0=backdrop/blur, binding1=noise
    VkDescriptorPool      acrylicDescriptorPool = VK_NULL_HANDLE;
    void*                 acrylicDescNoiseTex   = nullptr; // last noise bound (rewrite on change)
    VkImageView           acrylicDescBlurView   = VK_NULL_HANDLE; // last blur view bound
    bool                  backdropValid = false; // a capture from last frame exists
    bool                  acrylicReady  = false; // composite resources created OK

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
    VkRenderTarget* backdropRT = nullptr;            // #5: half-res captured backdrop for acrylic
    VkRenderTarget* blurResult = nullptr;            // #5 phase 2: final dual-Kawase blur (composite samples this)
    std::vector<VkRenderTarget*> blurChain;          // #5 phase 2: ping-pong down/up buffers

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
    bool CreateSurfaceForWindow();              // standalone + shared-device window
    bool PickPhysicalDevice();                  // standalone only
    bool CreateSwapchain();                     // standalone + shared-device window
    void DestroySwapchain();
    void RecreateSwapchain();
    bool CreateStandaloneRenderPass();
    bool CreateSyncAndCommands();               // standalone only
    bool CreateShaderModules();
    // gap #4: createLayouts=false when a secondary window has already ADOPTED the
    // owner's descriptor-set/pipeline layouts (then this only builds the pipelines).
    bool CreatePipelines(bool createLayouts = true);
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

    // Render-target helpers (brief 05).
    bool CreateOffscreenRenderPass(VkRenderTarget* rt);
    VulkanTexture* CreateRTSampleTexture(VkRenderTarget* rt);
    void BeginOffscreenRecording();   // lazily begin offscreenCmd, switch currentCmd
    void BeginRTPass(VkRenderTarget* rt);
    void EndRTPass();                 // end the active RT pass (classic/dynamic)
    void FlushOffscreen();            // submit offscreenCmd, fence-wait, restore mainCmd
    void DestroyRenderTarget(VkRenderTarget* rt); // assumes device idle

    // Acrylic helpers (#5).
    bool CreateAcrylicResources();       // shaders + layouts + composite/blur pipelines + load pass
    void DestroyAcrylicResources();
    void EnsureBackdropRT(int w, int h); // (re)create half-res backdrop target
    void EnsureBlurChain(int w, int h);            // (re)create ping-pong blur targets
    bool CreateKawasePipelines(VkRenderTarget* compatibleRT); // lazy, needs a compatible offscreen pass
    // Break the main pass, blit the content-behind, dual-Kawase blur it (all inline on
    // the current command buffer, in order), then resume the main pass with loadOp=LOAD.
    void CaptureBehindAndBlur();
    void InlineKawasePass(VkRenderTarget* dst, VkRenderTarget* src, bool down);
    void RecordDraw(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                    const unsigned int* indices, size_t indexCount,
                    void* textureHandle, const float* projectionMatrix,
                    const Color& textColor, bool isLines, float lineWidth);
};

} // namespace FluentUI
