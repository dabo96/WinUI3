// On Windows, enable the native Win32 surface entry points so the backend can
// build a VkSurfaceKHR directly from an HWND when SDL was compiled without
// Vulkan support. Must be defined before <vulkan/vulkan.h> (pulled by the header).
#if defined(_WIN32)
  #define VK_USE_PLATFORM_WIN32_KHR
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
#endif

#include "core/VulkanBackend.h"
#include "core/EmbeddedShadersVulkan.h"
#include "core/Context.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <array>

// Temporary init diagnostics (Info logs are suppressed by the default logger).
#define VKDBG(msg) do { std::fprintf(stderr, "[VK] %s\n", msg); std::fflush(stderr); } while (0)

namespace FluentUI {

// Log a non-fatal Vulkan error and continue. Used inside helpers that return bool.
#define VK_FAIL(expr, msg)                                                  \
    do {                                                                    \
        VkResult _r = (expr);                                               \
        if (_r != VK_SUCCESS) {                                             \
            Log(LogLevel::Error, "Vulkan: %s (VkResult=%d)", msg, (int)_r); \
            return false;                                                   \
        }                                                                   \
    } while (0)

static VkDeviceSize AlignUp(VkDeviceSize v, VkDeviceSize a) {
    return (v + a - 1) & ~(a - 1);
}

// sRGB → linear (IEC 61966-2-1). Used to pre-compensate UI colors when the
// color attachment is an sRGB format: the GPU re-encodes linear→sRGB on write,
// so feeding linear values makes the stored pixel match the authored sRGB color
// and lets alpha blending (shadows, AA text) happen correctly in linear space.
static inline float SrgbToLinear(float c) {
    return (c <= 0.04045f) ? (c / 12.92f)
                           : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

static bool IsSrgbFormat(VkFormat f) {
    switch (f) {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return true;
        default:
            return false;
    }
}

VulkanBackend::~VulkanBackend() { Shutdown(); }

// ───────────────────────────────────────────────────────────────────────────
// Init
// ───────────────────────────────────────────────────────────────────────────
bool VulkanBackend::Init(void* windowHandle, void* existingContext) {
    window = static_cast<SDL_Window*>(windowHandle);

    if (existingContext) {
        // ── Shared mode: reuse the engine's Vulkan objects ──
        auto* shared = static_cast<VulkanSharedContext*>(existingContext);
        const bool dyn = shared->dynamicRendering;
        // With dynamic rendering there is no VkRenderPass; we need colorFormat
        // instead to build pipelines via VkPipelineRenderingCreateInfo.
        if (!shared->device || !shared->physicalDevice || !shared->graphicsQueue ||
            (!dyn && !shared->renderPass)) {
            Log(LogLevel::Error, "Vulkan: shared context missing required handles");
            return false;
        }
        if (dyn && shared->colorFormat == 0) {
            Log(LogLevel::Error, "Vulkan: dynamic rendering requires colorFormat to be set");
            return false;
        }
        sharedMode          = true;
        ownsDevice          = false;
        ownsRenderPass      = false;
        useDynamicRendering = dyn;
        instance         = reinterpret_cast<VkInstance>(shared->instance);
        physicalDevice   = reinterpret_cast<VkPhysicalDevice>(shared->physicalDevice);
        device           = reinterpret_cast<VkDevice>(shared->device);
        graphicsQueue    = reinterpret_cast<VkQueue>(shared->graphicsQueue);
        queueFamilyIndex = shared->queueFamilyIndex;
        renderPass       = dyn ? VK_NULL_HANDLE
                               : reinterpret_cast<VkRenderPass>(shared->renderPass);
        colorFormat      = static_cast<VkFormat>(shared->colorFormat);
        depthFormat      = static_cast<VkFormat>(shared->depthFormat);
        stencilFormat    = static_cast<VkFormat>(shared->stencilFormat);
        srgbTarget       = IsSrgbFormat(colorFormat);
        if (dyn) {
            Log(LogLevel::Info, "Vulkan: shared mode using dynamic rendering (no VkRenderPass)");
        }

        // Diagnostics: surface the handles we actually received. A null/garbage
        // device here is the cause of "vkCreateShaderModule: Invalid device".
        std::fprintf(stderr,
            "[VK] shared handles: instance=%p physicalDevice=%p device=%p queue=%p "
            "qFamily=%u renderPass=%p colorFmt=%d samples=%d dynamicRendering=%d\n",
            (void*)instance, (void*)physicalDevice, (void*)device, (void*)graphicsQueue,
            queueFamilyIndex, (void*)renderPass, (int)colorFormat, (int)sampleCount, (int)dyn);
        std::fflush(stderr);
        if (!device) {
            Log(LogLevel::Error, "Vulkan shared: device handle is NULL — check that "
                "VulkanSharedContext::device holds the engine's VkDevice.");
            return false;
        }
        if (srgbTarget) {
            Log(LogLevel::Info, "Vulkan: shared sRGB target detected — linearizing UI colors to avoid double gamma");
        }
        sampleCount      = static_cast<VkSampleCountFlagBits>(
                               shared->sampleCount ? shared->sampleCount : 1);
        wideLinesSupported = false; // can't assume the engine enabled the feature

        // Common integration mistakes: a partially-filled struct. These fields
        // aren't strictly required to *init*, but a wrong value crashes later
        // (pipeline incompatible with the engine's render pass), so flag them now.
        if (shared->colorFormat == 0) {
            Log(LogLevel::Warning, "Vulkan shared: colorFormat is 0 — fill it with the engine's "
                "color attachment VkFormat (gamma handling + sanity depend on it).");
        }
        if (shared->sampleCount == 0) {
            Log(LogLevel::Warning, "Vulkan shared: sampleCount is 0 — defaulting to 1. It MUST match "
                "the engine render pass sample count or the UI pipelines will be incompatible.");
        }

        if (!CreateShaderModules()) return false;
        if (!CreatePipelines()) return false;
        if (!CreateDynamicBuffers()) return false;
        if (!CreateSamplerAndDescriptorInfra()) return false;

        // Upload pool for texture staging copies.
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pci.queueFamilyIndex = queueFamilyIndex;
        VK_FAIL(vkCreateCommandPool(device, &pci, nullptr, &uploadPool), "create upload pool");

        Log(LogLevel::Info, "Vulkan backend initialized (shared mode)");
        return true;
    }

    // ── Standalone mode: own everything ──
    // If you meant to render *inside* an existing engine, reaching here means no
    // VulkanSharedContext was supplied (existingContext == nullptr). The backend
    // will now create its OWN instance/device/surface/swapchain on this window —
    // which collides with an engine that already owns a swapchain for it
    // (typically VK_ERROR_NATIVE_WINDOW_IN_USE_KHR or a driver crash). For engine
    // integration, pass a populated VulkanSharedContext* as existingContext.
    Log(LogLevel::Warning, "Vulkan backend running STANDALONE (no shared context supplied) — "
        "creating its own instance/device/swapchain. If you intended engine integration, "
        "pass a VulkanSharedContext* to CreateContext().");
    VKDBG("Vulkan: STANDALONE mode (no shared context) — creating own swapchain");
    ownsDevice = true;
    VKDBG("CreateInstanceAndDevice...");
    if (!CreateInstanceAndDevice()) { Shutdown(); return false; }
    VKDBG("CreateSwapchain...");
    if (!CreateSwapchain())         { Shutdown(); return false; } // also creates render pass + framebuffers
    VKDBG("CreateShaderModules...");
    if (!CreateShaderModules())     { Shutdown(); return false; }
    VKDBG("CreatePipelines...");
    if (!CreatePipelines())         { Shutdown(); return false; }
    VKDBG("CreateDynamicBuffers...");
    if (!CreateDynamicBuffers())    { Shutdown(); return false; }
    VKDBG("CreateSamplerAndDescriptorInfra...");
    if (!CreateSamplerAndDescriptorInfra()) { Shutdown(); return false; }
    VKDBG("CreateSyncAndCommands...");
    if (!CreateSyncAndCommands())   { Shutdown(); return false; }

    VKDBG("standalone init DONE (backend ok, fonts next)");
    Log(LogLevel::Info, "Vulkan backend initialized (standalone)");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Standalone device / swapchain creation
// ───────────────────────────────────────────────────────────────────────────
bool VulkanBackend::CreateInstanceAndDevice() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "FluentUI";
    app.apiVersion = VK_API_VERSION_1_1; // negative-height viewport (maintenance1)

    // Instance extensions / surface mechanism.
    // IMPORTANT: when SDL was built without Vulkan, its public SDL_Vulkan_* calls
    // dereference NULL driver function pointers (→ crash). Only SDL_Vulkan_LoadLibrary
    // is NULL-safe, so use it as the gate: touch the other SDL_Vulkan_* functions
    // only if it succeeds; otherwise go straight to the native Win32 path.
    bool useSdlSurface = false;
    std::vector<const char*> exts;
    if (SDL_Vulkan_LoadLibrary(nullptr)) {
        uint32_t sdlExtCount = 0;
        const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
        if (sdlExts && sdlExtCount > 0) {
            exts.assign(sdlExts, sdlExts + sdlExtCount);
            useSdlSurface = true;
        }
    }
    if (!useSdlSurface) {
#if defined(_WIN32)
        exts = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
        Log(LogLevel::Info, "Vulkan: SDL lacks Vulkan support — using native Win32 surface");
#else
        Log(LogLevel::Error, "Vulkan: no surface mechanism available (SDL has no Vulkan support)");
        return false;
#endif
    }

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    ici.ppEnabledExtensionNames = exts.data();
    VK_FAIL(vkCreateInstance(&ici, nullptr, &instance), "create instance");
    VKDBG(useSdlSurface ? "  instance ok (SDL surface path)" : "  instance ok (native Win32 path)");

    if (useSdlSurface) {
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
            Log(LogLevel::Error, "Vulkan: SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
            return false;
        }
    } else {
#if defined(_WIN32)
        HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!hwnd) {
            Log(LogLevel::Error, "Vulkan: could not obtain HWND for native surface");
            return false;
        }
        VkWin32SurfaceCreateInfoKHR wci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        wci.hinstance = GetModuleHandle(nullptr);
        wci.hwnd = hwnd;
        VK_FAIL(vkCreateWin32SurfaceKHR(instance, &wci, nullptr, &surface), "create Win32 surface");
#endif
    }

    VKDBG("  surface ok");
    if (!PickPhysicalDevice()) return false;
    VKDBG("  physical device ok");

    // Logical device with swapchain extension + optional wideLines.
    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(physicalDevice, &supported);
    VkPhysicalDeviceFeatures enabled{};
    if (supported.wideLines) { enabled.wideLines = VK_TRUE; wideLinesSupported = true; }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = queueFamilyIndex;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExts;
    dci.pEnabledFeatures = &enabled;
    VK_FAIL(vkCreateDevice(physicalDevice, &dci, nullptr, &device), "create device");

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);
    VKDBG("  device + queue ok");
    return true;
}

bool VulkanBackend::PickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (count == 0) { Log(LogLevel::Error, "Vulkan: no physical devices"); return false; }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    for (VkPhysicalDevice dev : devices) {
        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qfCount, qfs.data());
        for (uint32_t i = 0; i < qfCount; ++i) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                physicalDevice = dev;
                queueFamilyIndex = i;
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(dev, &props);
                std::fprintf(stderr, "[VK]   GPU: %s (Vulkan %u.%u.%u)\n", props.deviceName,
                             VK_VERSION_MAJOR(props.apiVersion),
                             VK_VERSION_MINOR(props.apiVersion),
                             VK_VERSION_PATCH(props.apiVersion));
                std::fflush(stderr);
                Log(LogLevel::Info, "Vulkan: using device '%s'", props.deviceName);
                return true;
            }
        }
    }
    Log(LogLevel::Error, "Vulkan: no graphics+present queue family found");
    return false;
}

bool VulkanBackend::CreateSwapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);

    // Choose a format (prefer BGRA8 UNORM / sRGB nonlinear).
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &fmtCount, formats.data());
    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; break; }
    }
    colorFormat = chosen.format;
    srgbTarget = IsSrgbFormat(colorFormat); // normally UNORM here, but stay correct if the fallback picks sRGB

    // Extent.
    if (caps.currentExtent.width != UINT32_MAX) {
        swapExtent = caps.currentExtent;
    } else {
        swapExtent.width  = std::clamp(logicalViewport.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        swapExtent.height = std::clamp(logicalViewport.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (swapExtent.width == 0 || swapExtent.height == 0) {
        // Minimized — keep old swapchain; nothing to (re)create.
        return true;
    }

    std::fprintf(stderr, "[VK]   swapchain extent (physical px): %ux%u\n",
                 swapExtent.width, swapExtent.height);
    std::fflush(stderr);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = colorFormat;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = swapExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed, vsync
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;
    VK_FAIL(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain), "create swapchain");

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imgCount, nullptr);
    swapImages.resize(imgCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imgCount, swapImages.data());

    swapImageViews.resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = swapImages[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = colorFormat;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_FAIL(vkCreateImageView(device, &vci, nullptr, &swapImageViews[i]), "create swap image view");
    }

    if (renderPass == VK_NULL_HANDLE) {
        if (!CreateStandaloneRenderPass()) return false;
    }

    framebuffers.resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &swapImageViews[i];
        fci.width = swapExtent.width;
        fci.height = swapExtent.height;
        fci.layers = 1;
        VK_FAIL(vkCreateFramebuffer(device, &fci, nullptr, &framebuffers[i]), "create framebuffer");
    }

    // One render-finished semaphore per swapchain image (avoids reuse hazard).
    renderFinished.resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i) {
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_FAIL(vkCreateSemaphore(device, &si, nullptr, &renderFinished[i]), "create renderFinished semaphore");
    }

    logicalViewport = swapExtent;
    return true;
}

bool VulkanBackend::CreateStandaloneRenderPass() {
    VkAttachmentDescription color{};
    color.format = colorFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    VK_FAIL(vkCreateRenderPass(device, &rpci, nullptr, &renderPass), "create render pass");
    ownsRenderPass = true;
    sampleCount = VK_SAMPLE_COUNT_1_BIT;
    return true;
}

bool VulkanBackend::CreateSyncAndCommands() {
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = queueFamilyIndex;
    VK_FAIL(vkCreateCommandPool(device, &pci, nullptr, &commandPool), "create command pool");

    VkCommandPoolCreateInfo upci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    upci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    upci.queueFamilyIndex = queueFamilyIndex;
    VK_FAIL(vkCreateCommandPool(device, &upci, nullptr, &uploadPool), "create upload pool");

    for (int i = 0; i < kFramesInFlight; ++i) {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = commandPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VK_FAIL(vkAllocateCommandBuffers(device, &ai, &frames[i].cmd), "alloc frame cmd");

        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_FAIL(vkCreateSemaphore(device, &si, nullptr, &frames[i].imageAvailable), "create imageAvailable semaphore");

        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT; // first wait returns immediately
        VK_FAIL(vkCreateFence(device, &fi, nullptr, &frames[i].inFlight), "create inFlight fence");
    }
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Shaders / pipelines
// ───────────────────────────────────────────────────────────────────────────
VkShaderModule VulkanBackend::MakeShaderModule(const uint32_t* code, size_t bytes) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes;
    ci.pCode = code;
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &m) != VK_SUCCESS) {
        Log(LogLevel::Error, "Vulkan: failed to create shader module");
        return VK_NULL_HANDLE;
    }
    return m;
}

bool VulkanBackend::CreateShaderModules() {
    vertModule = MakeShaderModule(ShadersVK::UI_Vert,    ShadersVK::UI_VertSize);
    basicFrag  = MakeShaderModule(ShadersVK::Basic_Frag, ShadersVK::Basic_FragSize);
    textFrag   = MakeShaderModule(ShadersVK::Text_Frag,  ShadersVK::Text_FragSize);
    msdfFrag   = MakeShaderModule(ShadersVK::MSDF_Frag,  ShadersVK::MSDF_FragSize);
    imageFrag  = MakeShaderModule(ShadersVK::Image_Frag, ShadersVK::Image_FragSize);
    sdfVertModule = MakeShaderModule(ShadersVK::SDFRect_Vert, ShadersVK::SDFRect_VertSize);
    sdfFrag       = MakeShaderModule(ShadersVK::SDFRect_Frag, ShadersVK::SDFRect_FragSize);
    return vertModule && basicFrag && textFrag && msdfFrag && imageFrag &&
           sdfVertModule && sdfFrag;
}

bool VulkanBackend::CreatePipelines() {
    // Descriptor set layout: one combined image sampler at binding 0 (fragment).
    VkDescriptorSetLayoutBinding b{};
    b.binding = 0;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dlci.bindingCount = 1;
    dlci.pBindings = &b;
    VK_FAIL(vkCreateDescriptorSetLayout(device, &dlci, nullptr, &texSetLayout), "create set layout");

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo plNoTex{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plNoTex.pushConstantRangeCount = 1;
    plNoTex.pPushConstantRanges = &pcr;
    VK_FAIL(vkCreatePipelineLayout(device, &plNoTex, nullptr, &layoutNoTex), "create layoutNoTex");

    VkPipelineLayoutCreateInfo plTex{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plTex.setLayoutCount = 1;
    plTex.pSetLayouts = &texSetLayout;
    plTex.pushConstantRangeCount = 1;
    plTex.pPushConstantRanges = &pcr;
    VK_FAIL(vkCreatePipelineLayout(device, &plTex, nullptr, &layoutTex), "create layoutTex");

    pipeBasic = MakePipeline(basicFrag, layoutNoTex, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
    pipeText  = MakePipeline(textFrag,  layoutTex,   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
    pipeMSDF  = MakePipeline(msdfFrag,  layoutTex,   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
    pipeImage = MakePipeline(imageFrag, layoutTex,   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
    pipeLines = MakePipeline(basicFrag, layoutNoTex, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, wideLinesSupported);
    pipeSDF   = MakeSDFPipeline(); // SDF uses layoutNoTex (no texture, push constants only)
    return pipeBasic && pipeText && pipeMSDF && pipeImage && pipeLines && pipeSDF;
}

VkPipeline VulkanBackend::MakePipeline(VkShaderModule frag, VkPipelineLayout layout,
                                       VkPrimitiveTopology topology, bool dynamicLineWidth) {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind{};
    bind.binding = 0;
    bind.stride = sizeof(RenderVertex);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(RenderVertex, x)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RenderVertex, r)};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(RenderVertex, u)};

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = topology;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = sampleCount;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    // Match the GL blend: src=SRC_ALPHA, dst=ONE_MINUS_SRC_ALPHA (color and alpha).
    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    std::vector<VkDynamicState> dyn = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    if (dynamicLineWidth) dyn.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
    VkPipelineDynamicStateCreateInfo dynci{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynci.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dynci.pDynamicStates = dyn.data();

    VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pDepthStencilState = &ds;
    gpci.pColorBlendState = &cb;
    gpci.pDynamicState = &dynci;
    gpci.layout = layout;
    gpci.subpass = 0;

    // Dynamic rendering: no VkRenderPass; declare the attachment formats via
    // VkPipelineRenderingCreateInfo so the pipeline is compatible with the engine's
    // vkCmdBeginRendering. Must match the engine's VkRenderingInfo at draw time.
    VkPipelineRenderingCreateInfo prci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    VkFormat colorFmt = colorFormat; // needs a stable address for pColorAttachmentFormats
    if (useDynamicRendering) {
        prci.colorAttachmentCount    = 1;
        prci.pColorAttachmentFormats = &colorFmt;
        prci.depthAttachmentFormat   = depthFormat;   // VK_FORMAT_UNDEFINED if none
        prci.stencilAttachmentFormat = stencilFormat; // VK_FORMAT_UNDEFINED if none
        gpci.pNext = &prci;
        gpci.renderPass = VK_NULL_HANDLE;
    } else {
        gpci.renderPass = renderPass;
    }

    VkPipeline pipe = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipe) != VK_SUCCESS) {
        Log(LogLevel::Error, "Vulkan: failed to create graphics pipeline");
        return VK_NULL_HANDLE;
    }
    return pipe;
}

// Instanced SDF pipeline (brief 01). Two vertex bindings: binding 0 = per-vertex
// unit quad (vec2); binding 1 = per-instance SDFInstance. Uses layoutNoTex.
VkPipeline VulkanBackend::MakeSDFPipeline() {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = sdfVertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = sdfFrag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binds[2]{};
    binds[0].binding = 0;
    binds[0].stride = sizeof(float) * 2;          // aQuad
    binds[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binds[1].binding = 1;
    binds[1].stride = sizeof(SDFInstance);
    binds[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription attrs[7]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT,       0};
    attrs[1] = {1, 1, VK_FORMAT_R32G32_SFLOAT,       offsetof(SDFInstance, cx)};
    attrs[2] = {2, 1, VK_FORMAT_R32G32_SFLOAT,       offsetof(SDFInstance, hx)};
    attrs[3] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SDFInstance, radius)};
    attrs[4] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SDFInstance, fillR)};
    attrs[5] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SDFInstance, borderR)};
    attrs[6] = {6, 1, VK_FORMAT_R32_SFLOAT,          offsetof(SDFInstance, revealIntensity)};

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 2;
    vi.pVertexBindingDescriptions = binds;
    vi.vertexAttributeDescriptionCount = 7;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = sampleCount;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkDynamicState dynStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynci{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynci.dynamicStateCount = 2;
    dynci.pDynamicStates = dynStates;

    VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pDepthStencilState = &ds;
    gpci.pColorBlendState = &cb;
    gpci.pDynamicState = &dynci;
    gpci.layout = layoutNoTex;
    gpci.subpass = 0;

    VkPipelineRenderingCreateInfo prci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    VkFormat colorFmt = colorFormat;
    if (useDynamicRendering) {
        prci.colorAttachmentCount    = 1;
        prci.pColorAttachmentFormats = &colorFmt;
        prci.depthAttachmentFormat   = depthFormat;
        prci.stencilAttachmentFormat = stencilFormat;
        gpci.pNext = &prci;
        gpci.renderPass = VK_NULL_HANDLE;
    } else {
        gpci.renderPass = renderPass;
    }

    VkPipeline pipe = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipe) != VK_SUCCESS) {
        Log(LogLevel::Error, "Vulkan: failed to create SDF graphics pipeline");
        return VK_NULL_HANDLE;
    }
    return pipe;
}

// ───────────────────────────────────────────────────────────────────────────
// Buffers / memory
// ───────────────────────────────────────────────────────────────────────────
uint32_t VulkanBackend::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    Log(LogLevel::Error, "Vulkan: no suitable memory type");
    return 0;
}

bool VulkanBackend::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_FAIL(vkCreateBuffer(device, &bci, nullptr, &buf), "create buffer");

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buf, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
    VK_FAIL(vkAllocateMemory(device, &ai, nullptr, &mem), "allocate buffer memory");
    VK_FAIL(vkBindBufferMemory(device, buf, mem, 0), "bind buffer memory");
    return true;
}

bool VulkanBackend::CreateDynamicBuffers() {
    const VkMemoryPropertyFlags hostProps =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (int i = 0; i < kRingSize; ++i) {
        if (!CreateBuffer(kVertexBytesPerSlot, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostProps,
                          ring[i].vbo, ring[i].vboMem)) return false;
        if (!CreateBuffer(kIndexBytesPerSlot, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, hostProps,
                          ring[i].ebo, ring[i].eboMem)) return false;
        VK_FAIL(vkMapMemory(device, ring[i].vboMem, 0, kVertexBytesPerSlot, 0, &ring[i].vboMapped), "map vbo");
        VK_FAIL(vkMapMemory(device, ring[i].eboMem, 0, kIndexBytesPerSlot, 0, &ring[i].eboMapped), "map ebo");
    }

    // Static unit-quad geometry for the SDF pipeline (brief 01). Host-visible and
    // filled once; tiny so the upload cost is negligible.
    {
        const float quadVerts[8] = {
            -1.0f, -1.0f,   1.0f, -1.0f,   1.0f, 1.0f,   -1.0f, 1.0f
        };
        const uint32_t quadIdx[6] = { 0, 1, 2, 0, 2, 3 };
        if (!CreateBuffer(sizeof(quadVerts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostProps,
                          sdfQuadVbo, sdfQuadVboMem)) return false;
        if (!CreateBuffer(sizeof(quadIdx), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, hostProps,
                          sdfQuadIbo, sdfQuadIboMem)) return false;
        void* p = nullptr;
        VK_FAIL(vkMapMemory(device, sdfQuadVboMem, 0, sizeof(quadVerts), 0, &p), "map sdf quad vbo");
        std::memcpy(p, quadVerts, sizeof(quadVerts));
        vkUnmapMemory(device, sdfQuadVboMem);
        VK_FAIL(vkMapMemory(device, sdfQuadIboMem, 0, sizeof(quadIdx), 0, &p), "map sdf quad ibo");
        std::memcpy(p, quadIdx, sizeof(quadIdx));
        vkUnmapMemory(device, sdfQuadIboMem);
    }
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// Sampler / descriptors
// ───────────────────────────────────────────────────────────────────────────
bool VulkanBackend::CreateSamplerAndDescriptorInfra() {
    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.maxLod = 0.0f;
    VK_FAIL(vkCreateSampler(device, &sci, nullptr, &sampler), "create sampler");
    return true; // descriptor pools are created lazily on first allocation
}

VkDescriptorSet VulkanBackend::AllocateTextureDescriptor(VkDescriptorPool& outPool) {
    outPool = VK_NULL_HANDLE;
    auto tryAlloc = [&](VkDescriptorPool pool) -> VkDescriptorSet {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &texSetLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(device, &ai, &set) == VK_SUCCESS) return set;
        return VK_NULL_HANDLE;
    };

    if (!descriptorPools.empty()) {
        if (VkDescriptorSet s = tryAlloc(descriptorPools.back())) {
            outPool = descriptorPools.back();
            return s;
        }
    }

    // Out of space (or first allocation): create a new pool. FREE_DESCRIPTOR_SET_BIT
    // lets us reclaim individual sets via vkFreeDescriptorSets (needed so frequently
    // re-registered textures — e.g. an editor viewport on resize — don't leak).
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128};
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pci.maxSets = 128;
    pci.poolSizeCount = 1;
    pci.pPoolSizes = &ps;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &pci, nullptr, &pool) != VK_SUCCESS) {
        Log(LogLevel::Error, "Vulkan: failed to create descriptor pool");
        return VK_NULL_HANDLE;
    }
    descriptorPools.push_back(pool);
    VkDescriptorSet s = tryAlloc(pool);
    if (s) outPool = pool;
    return s;
}

// ───────────────────────────────────────────────────────────────────────────
// Single-time command helpers (texture uploads)
// ───────────────────────────────────────────────────────────────────────────
VkCommandBuffer VulkanBackend::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = uploadPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &ai, &cmd);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void VulkanBackend::EndSingleTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    // Wait on a dedicated fence instead of vkQueueWaitIdle: in shared mode the
    // queue belongs to the engine, so draining ALL of its in-flight work on every
    // upload is wasteful. The fence scopes the wait to just this upload.
    // NOTE: VkQueue submission still requires external synchronization — the host
    // must not submit to this queue from another thread while UI uploads run.
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(device, &fci, nullptr, &fence) == VK_SUCCESS) {
        vkQueueSubmit(graphicsQueue, 1, &si, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device, fence, nullptr);
    } else {
        vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue); // fallback if fence creation failed
    }
    vkFreeCommandBuffers(device, uploadPool, 1, &cmd);
}

void VulkanBackend::TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                          VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else { // UNDEFINED -> SHADER_READ_ONLY (empty atlas)
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

// ───────────────────────────────────────────────────────────────────────────
// Textures
// ───────────────────────────────────────────────────────────────────────────
void* VulkanBackend::CreateTexture(int width, int height, const void* data, bool alphaOnly) {
    VKDBG("CreateTexture");
    auto* t = new VulkanTexture();
    t->width = width;
    t->height = height;
    t->alphaOnly = alphaOnly;
    const VkFormat fmt = alphaOnly ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * (alphaOnly ? 1 : 4);

    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = fmt;
    ici.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &ici, nullptr, &t->image) != VK_SUCCESS) {
        Log(LogLevel::Error, "Vulkan: failed to create texture image"); delete t; return nullptr;
    }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, t->image, &req);
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &mai, nullptr, &t->memory);
    vkBindImageMemory(device, t->image, t->memory, 0);

    if (data && bytes > 0) {
        VkBuffer staging; VkDeviceMemory stagingMem;
        CreateBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     staging, stagingMem);
        void* mapped = nullptr;
        vkMapMemory(device, stagingMem, 0, bytes, 0, &mapped);
        std::memcpy(mapped, data, bytes);
        vkUnmapMemory(device, stagingMem);

        VkCommandBuffer cmd = BeginSingleTimeCommands();
        TransitionImageLayout(cmd, t->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
        vkCmdCopyBufferToImage(cmd, staging, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        TransitionImageLayout(cmd, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        EndSingleTimeCommands(cmd);

        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
    } else {
        // Empty texture (e.g. a glyph atlas filled later by UpdateTexture).
        VkCommandBuffer cmd = BeginSingleTimeCommands();
        TransitionImageLayout(cmd, t->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        EndSingleTimeCommands(cmd);
    }

    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = t->image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = fmt;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(device, &vci, nullptr, &t->view);

    t->descriptor = AllocateTextureDescriptor(t->descriptorPool);
    if (t->descriptor) {
        VkDescriptorImageInfo dii{};
        dii.sampler = sampler;
        dii.imageView = t->view;
        dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = t->descriptor;
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &dii;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    textures.push_back(t);
    return t;
}

void* VulkanBackend::RegisterExternalTexture(void* nativeView, void* samplerHandle, int layout) {
    if (!nativeView) {
        Log(LogLevel::Error, "Vulkan: RegisterExternalTexture called with null VkImageView");
        return nullptr;
    }
    auto* t = new VulkanTexture();
    t->external = true;
    t->view = reinterpret_cast<VkImageView>(nativeView);

    VkSampler smp = samplerHandle ? reinterpret_cast<VkSampler>(samplerHandle) : sampler;
    VkImageLayout imgLayout = layout ? static_cast<VkImageLayout>(layout)
                                     : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    t->descriptor = AllocateTextureDescriptor(t->descriptorPool);
    if (!t->descriptor) { delete t; return nullptr; }

    VkDescriptorImageInfo dii{};
    dii.sampler = smp;
    dii.imageView = t->view;
    dii.imageLayout = imgLayout;
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = t->descriptor;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &dii;
    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);

    textures.push_back(t);
    return t;
}

void VulkanBackend::UpdateTexture(void* textureHandle, int x, int y, int width, int height, const void* data) {
    if (!textureHandle || !data) return;
    auto* t = static_cast<VulkanTexture*>(textureHandle);
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * (t->alphaOnly ? 1 : 4);
    if (bytes == 0) return;

    VkBuffer staging; VkDeviceMemory stagingMem;
    CreateBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging, stagingMem);
    void* mapped = nullptr;
    vkMapMemory(device, stagingMem, 0, bytes, 0, &mapped);
    std::memcpy(mapped, data, bytes);
    vkUnmapMemory(device, stagingMem);

    VkCommandBuffer cmd = BeginSingleTimeCommands();
    TransitionImageLayout(cmd, t->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {x, y, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(cmd, staging, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    TransitionImageLayout(cmd, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EndSingleTimeCommands(cmd);

    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);
}

void VulkanBackend::DeleteTexture(void* textureHandle) {
    if (!textureHandle) return;
    auto* t = static_cast<VulkanTexture*>(textureHandle);
    if (device) vkDeviceWaitIdle(device); // ensure GPU no longer samples it
    if (!t->external) {
        // Only destroy resources we created. External textures wrap host-owned
        // image/view/memory — the engine owns and frees those.
        if (t->view) vkDestroyImageView(device, t->view, nullptr);
        if (t->image) vkDestroyImage(device, t->image, nullptr);
        if (t->memory) vkFreeMemory(device, t->memory, nullptr);
    }
    // Free the descriptor set back to its pool (pools use FREE_DESCRIPTOR_SET_BIT).
    // Safe here because we just waited for the device to go idle above.
    if (t->descriptor && t->descriptorPool) {
        vkFreeDescriptorSets(device, t->descriptorPool, 1, &t->descriptor);
    }
    auto it = std::find(textures.begin(), textures.end(), t);
    if (it != textures.end()) textures.erase(it);
    delete t;
}

// ───────────────────────────────────────────────────────────────────────────
// Frame
// ───────────────────────────────────────────────────────────────────────────
void VulkanBackend::SetFrameCommandBuffer(void* cmdBuffer) {
    externalCmd = reinterpret_cast<VkCommandBuffer>(cmdBuffer);
}

void VulkanBackend::SetFullViewportAndScissor() {
    if (!currentCmd) return;
    const float w = static_cast<float>(sharedMode ? logicalViewport.width  : swapExtent.width);
    const float h = static_cast<float>(sharedMode ? logicalViewport.height : swapExtent.height);
    // Negative-height viewport flips Y so the GL ortho matrix works unchanged.
    VkViewport vp{0.0f, h, w, -h, 0.0f, 1.0f};
    vkCmdSetViewport(currentCmd, 0, 1, &vp);
    VkRect2D sc{{0, 0}, {static_cast<uint32_t>(w), static_cast<uint32_t>(h)}};
    vkCmdSetScissor(currentCmd, 0, 1, &sc);
}

void VulkanBackend::ApplyScissor() {
    if (!currentCmd) return;
    // Framebuffer size in physical pixels.
    const int fbW = static_cast<int>(sharedMode ? logicalViewport.width  : swapExtent.width);
    const int fbH = static_cast<int>(sharedMode ? logicalViewport.height : swapExtent.height);
    // Clip rects arrive in *logical* coordinates (same space as the ortho matrix).
    // The geometry is stretched to the physical framebuffer by the viewport, so the
    // scissor must scale logical → physical too, or it won't line up on HiDPI.
    const float sx = (logicalViewport.width  > 0) ? static_cast<float>(fbW) / logicalViewport.width  : 1.0f;
    const float sy = (logicalViewport.height > 0) ? static_cast<float>(fbH) / logicalViewport.height : 1.0f;
    VkRect2D sc;
    if (clipStack.empty()) {
        sc = {{0, 0}, {static_cast<uint32_t>(fbW), static_cast<uint32_t>(fbH)}};
    } else {
        const ClipRect& r = clipStack.back();
        int x = std::clamp(static_cast<int>(std::lround(r.x * sx)), 0, fbW);
        int y = std::clamp(static_cast<int>(std::lround(r.y * sy)), 0, fbH);
        int w = std::clamp(static_cast<int>(std::lround(r.w * sx)), 0, fbW - x);
        int h = std::clamp(static_cast<int>(std::lround(r.h * sy)), 0, fbH - y);
        sc = {{x, y}, {static_cast<uint32_t>(w), static_cast<uint32_t>(h)}};
    }
    vkCmdSetScissor(currentCmd, 0, 1, &sc);
}

void VulkanBackend::BeginFrame(const Color& clearColor) {
    clipStack.clear();
    ringOverflowWarned = false;

    if (sharedMode) {
        currentCmd = externalCmd;
        if (!currentCmd) {
            if (!warnedNoExternalCmd) {
                Log(LogLevel::Warning, "Vulkan shared: no external command buffer set — UI will not "
                    "render. Call SetFrameCommandBuffer(cmd) each frame, inside the engine's active "
                    "render pass, before Render().");
                warnedNoExternalCmd = true;
            }
            return;
        }
        currentRing = (currentRing + 1) % kRingSize;
        ring[currentRing].vboOffset = 0;
        ring[currentRing].eboOffset = 0;
        SetFullViewportAndScissor(); // engine is already inside its render pass
        return;
    }

    // Standalone: drive the whole frame.
    FrameSync& fr = frames[currentFrame];
    vkWaitForFences(device, 1, &fr.inFlight, VK_TRUE, UINT64_MAX);

    if (swapchainDirty) { RecreateSwapchain(); swapchainDirty = false; }

    if (swapchain == VK_NULL_HANDLE) {
        // Window is minimized (zero extent) — skip the frame.
        currentCmd = VK_NULL_HANDLE;
        return;
    }

    VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, fr.imageAvailable,
                                         VK_NULL_HANDLE, &currentImageIndex);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, fr.imageAvailable,
                                    VK_NULL_HANDLE, &currentImageIndex);
    }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
        currentCmd = VK_NULL_HANDLE; // skip this frame
        return;
    }
    vkResetFences(device, 1, &fr.inFlight);

    currentRing = currentFrame; // kRingSize >= kFramesInFlight, slot guarded by fence
    ring[currentRing].vboOffset = 0;
    ring[currentRing].eboOffset = 0;

    currentCmd = fr.cmd;
    vkResetCommandBuffer(currentCmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(currentCmd, &bi);

    VkClearValue clear{};
    clear.color = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};
    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = renderPass;
    rbi.framebuffer = framebuffers[currentImageIndex];
    rbi.renderArea = {{0, 0}, swapExtent};
    rbi.clearValueCount = 1;
    rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(currentCmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);

    SetFullViewportAndScissor();
}

void VulkanBackend::EndFrame() {
    if (sharedMode) {
        // The engine ends its render pass, submits, and presents.
        currentCmd = VK_NULL_HANDLE;
        return;
    }
    if (!currentCmd) return; // frame was skipped

    vkCmdEndRenderPass(currentCmd);
    vkEndCommandBuffer(currentCmd);

    FrameSync& fr = frames[currentFrame];
    VkSemaphore signalSem = renderFinished[currentImageIndex];
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &fr.imageAvailable;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &currentCmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &signalSem;
    vkQueueSubmit(graphicsQueue, 1, &si, fr.inFlight);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &signalSem;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain;
    pi.pImageIndices = &currentImageIndex;
    VkResult pres = vkQueuePresentKHR(graphicsQueue, &pi);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) swapchainDirty = true;

    currentFrame = (currentFrame + 1) % kFramesInFlight;
    currentCmd = VK_NULL_HANDLE;
}

void VulkanBackend::SetViewport(int width, int height) {
    if (width <= 0 || height <= 0) return;
    logicalViewport.width = static_cast<uint32_t>(width);
    logicalViewport.height = static_cast<uint32_t>(height);
    if (!sharedMode && swapchain != VK_NULL_HANDLE &&
        (swapExtent.width != logicalViewport.width || swapExtent.height != logicalViewport.height)) {
        swapchainDirty = true;
    }
}

void VulkanBackend::PushClipRect(int x, int y, int width, int height) {
    clipStack.push_back({x, y, width, height});
    ApplyScissor();
}

void VulkanBackend::PopClipRect() {
    if (!clipStack.empty()) clipStack.pop_back();
    ApplyScissor();
}

// ───────────────────────────────────────────────────────────────────────────
// Draw
// ───────────────────────────────────────────────────────────────────────────
void VulkanBackend::DrawBatch(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                              const unsigned int* indices, size_t indexCount,
                              void* textureHandle, const float* projectionMatrix,
                              const Color& textColor) {
    RecordDraw(type, vertices, vertexCount, indices, indexCount, textureHandle,
               projectionMatrix, textColor, /*isLines=*/false, 1.0f);
}

void VulkanBackend::DrawLines(const RenderVertex* vertices, size_t vertexCount,
                              float width, const float* projectionMatrix) {
    RecordDraw(ShaderType::Basic, vertices, vertexCount, nullptr, 0, nullptr,
               projectionMatrix, Color(1, 1, 1, 1), /*isLines=*/true, width);
}

void VulkanBackend::DrawSDFInstances(const SDFInstance* instances, size_t count,
                                     const float* projectionMatrix,
                                     const float* revealCursor) {
    if (!currentCmd || count == 0 || !instances) return;
    DynBuffer& rb = ring[currentRing];

    const VkDeviceSize bytes = count * sizeof(SDFInstance);
    if (rb.vboOffset + bytes > kVertexBytesPerSlot) {
        if (!ringOverflowWarned) {
            Log(LogLevel::Warning, "Vulkan: vertex ring slot overflow — dropping SDF draws this frame");
            ringOverflowWarned = true;
        }
        return;
    }
    const VkDeviceSize instLocal = rb.vboOffset;

    if (srgbTarget) {
        // Linearize the instance fill/border colors so the GPU's sRGB write-encode
        // reproduces the authored color (parity with the vertex-color path).
        scratchInstances.assign(instances, instances + count);
        for (auto& s : scratchInstances) {
            s.fillR = SrgbToLinear(s.fillR); s.fillG = SrgbToLinear(s.fillG); s.fillB = SrgbToLinear(s.fillB);
            s.borderR = SrgbToLinear(s.borderR); s.borderG = SrgbToLinear(s.borderG); s.borderB = SrgbToLinear(s.borderB);
        }
        std::memcpy(static_cast<char*>(rb.vboMapped) + instLocal, scratchInstances.data(), bytes);
    } else {
        std::memcpy(static_cast<char*>(rb.vboMapped) + instLocal, instances, bytes);
    }
    rb.vboOffset = AlignUp(rb.vboOffset + bytes, 16);

    vkCmdBindPipeline(currentCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeSDF);

    PushConstants pc{};
    std::memcpy(pc.projection, projectionMatrix, sizeof(pc.projection));
    pc.pxRange = 4.0f;
    if (revealCursor) { pc.reveal[0] = revealCursor[0]; pc.reveal[1] = revealCursor[1]; pc.reveal[2] = revealCursor[2]; }
    vkCmdPushConstants(currentCmd, layoutNoTex, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    // Binding 0: static quad. Binding 1: per-instance data in the ring buffer.
    VkBuffer vbos[2] = { sdfQuadVbo, rb.vbo };
    VkDeviceSize offs[2] = { 0, instLocal };
    vkCmdBindVertexBuffers(currentCmd, 0, 2, vbos, offs);
    vkCmdBindIndexBuffer(currentCmd, sdfQuadIbo, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(currentCmd, 6, static_cast<uint32_t>(count), 0, 0, 0);
}

void VulkanBackend::RecordDraw(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                               const unsigned int* indices, size_t indexCount,
                               void* textureHandle, const float* projectionMatrix,
                               const Color& textColor, bool isLines, float lineWidth) {
    if (!currentCmd || vertexCount == 0) return;
    DynBuffer& rb = ring[currentRing];

    const VkDeviceSize vboBytes = vertexCount * sizeof(RenderVertex);
    if (rb.vboOffset + vboBytes > kVertexBytesPerSlot) {
        if (!ringOverflowWarned) {
            Log(LogLevel::Warning, "Vulkan: vertex ring slot overflow — dropping draws this frame");
            ringOverflowWarned = true;
        }
        return;
    }
    const VkDeviceSize vboLocal = rb.vboOffset;
    if (srgbTarget) {
        // Pre-convert vertex colors sRGB→linear so the GPU's sRGB write-encode
        // reproduces the authored color (covers fills, lines and soft shadows).
        scratchVerts.assign(vertices, vertices + vertexCount);
        for (auto& v : scratchVerts) {
            v.r = SrgbToLinear(v.r);
            v.g = SrgbToLinear(v.g);
            v.b = SrgbToLinear(v.b);
        }
        std::memcpy(static_cast<char*>(rb.vboMapped) + vboLocal, scratchVerts.data(), vboBytes);
    } else {
        std::memcpy(static_cast<char*>(rb.vboMapped) + vboLocal, vertices, vboBytes);
    }
    rb.vboOffset = AlignUp(rb.vboOffset + vboBytes, 16);

    // Select pipeline + layout.
    VkPipeline pipe; VkPipelineLayout layout; bool hasTex = false;
    if (isLines) {
        pipe = pipeLines; layout = layoutNoTex;
    } else switch (type) {
        case ShaderType::Basic: pipe = pipeBasic; layout = layoutNoTex; break;
        case ShaderType::Text:  pipe = pipeText;  layout = layoutTex; hasTex = true; break;
        case ShaderType::MSDF:  pipe = pipeMSDF;  layout = layoutTex; hasTex = true; break;
        case ShaderType::Image: pipe = pipeImage; layout = layoutTex; hasTex = true; break;
        default: pipe = pipeBasic; layout = layoutNoTex; break;
    }

    vkCmdBindPipeline(currentCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    if (isLines && wideLinesSupported) vkCmdSetLineWidth(currentCmd, lineWidth);

    PushConstants pc{};
    std::memcpy(pc.projection, projectionMatrix, sizeof(pc.projection));
    if (srgbTarget) {
        // text.frag / msdf.frag tint by this push-constant color — linearize it too.
        pc.textColor[0] = SrgbToLinear(textColor.r);
        pc.textColor[1] = SrgbToLinear(textColor.g);
        pc.textColor[2] = SrgbToLinear(textColor.b);
    } else {
        pc.textColor[0] = textColor.r; pc.textColor[1] = textColor.g;
        pc.textColor[2] = textColor.b;
    }
    pc.textColor[3] = textColor.a;
    pc.pxRange = 4.0f;
    vkCmdPushConstants(currentCmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    if (hasTex && textureHandle) {
        auto* t = static_cast<VulkanTexture*>(textureHandle);
        if (t->descriptor)
            vkCmdBindDescriptorSets(currentCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layoutTex,
                                    0, 1, &t->descriptor, 0, nullptr);
    }

    VkDeviceSize vbOff = vboLocal;
    vkCmdBindVertexBuffers(currentCmd, 0, 1, &rb.vbo, &vbOff);

    if (indices && indexCount > 0) {
        const VkDeviceSize eboBytes = indexCount * sizeof(unsigned int);
        if (rb.eboOffset + eboBytes > kIndexBytesPerSlot) {
            if (!ringOverflowWarned) {
                Log(LogLevel::Warning, "Vulkan: index ring slot overflow — dropping draws this frame");
                ringOverflowWarned = true;
            }
            return;
        }
        const VkDeviceSize eboLocal = rb.eboOffset;
        std::memcpy(static_cast<char*>(rb.eboMapped) + eboLocal, indices, eboBytes);
        rb.eboOffset = AlignUp(rb.eboOffset + eboBytes, 16);
        vkCmdBindIndexBuffer(currentCmd, rb.ebo, eboLocal, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(currentCmd, static_cast<uint32_t>(indexCount), 1, 0, 0, 0);
    } else {
        vkCmdDraw(currentCmd, static_cast<uint32_t>(vertexCount), 1, 0, 0);
    }
}

// ───────────────────────────────────────────────────────────────────────────
// Swapchain recreation / teardown
// ───────────────────────────────────────────────────────────────────────────
void VulkanBackend::DestroySwapchain() {
    for (VkFramebuffer fb : framebuffers) if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers.clear();
    for (VkSemaphore s : renderFinished) if (s) vkDestroySemaphore(device, s, nullptr);
    renderFinished.clear();
    for (VkImageView v : swapImageViews) if (v) vkDestroyImageView(device, v, nullptr);
    swapImageViews.clear();
    swapImages.clear();
    if (swapchain) { vkDestroySwapchainKHR(device, swapchain, nullptr); swapchain = VK_NULL_HANDLE; }
}

void VulkanBackend::RecreateSwapchain() {
    if (sharedMode) return;
    vkDeviceWaitIdle(device);
    DestroySwapchain();
    CreateSwapchain(); // render pass is preserved across recreations
}

void VulkanBackend::Shutdown() {
    if (device) vkDeviceWaitIdle(device);

    for (auto* t : textures) {
        if (!t->external) {
            if (t->view) vkDestroyImageView(device, t->view, nullptr);
            if (t->image) vkDestroyImage(device, t->image, nullptr);
            if (t->memory) vkFreeMemory(device, t->memory, nullptr);
        }
        delete t;
    }
    textures.clear();

    if (sampler) { vkDestroySampler(device, sampler, nullptr); sampler = VK_NULL_HANDLE; }
    for (VkDescriptorPool p : descriptorPools) vkDestroyDescriptorPool(device, p, nullptr);
    descriptorPools.clear();

    for (int i = 0; i < kRingSize; ++i) {
        if (ring[i].vboMem) { vkUnmapMemory(device, ring[i].vboMem); }
        if (ring[i].eboMem) { vkUnmapMemory(device, ring[i].eboMem); }
        if (ring[i].vbo) vkDestroyBuffer(device, ring[i].vbo, nullptr);
        if (ring[i].ebo) vkDestroyBuffer(device, ring[i].ebo, nullptr);
        if (ring[i].vboMem) vkFreeMemory(device, ring[i].vboMem, nullptr);
        if (ring[i].eboMem) vkFreeMemory(device, ring[i].eboMem, nullptr);
        ring[i] = DynBuffer{};
    }

    if (sdfQuadVbo) { vkDestroyBuffer(device, sdfQuadVbo, nullptr); sdfQuadVbo = VK_NULL_HANDLE; }
    if (sdfQuadVboMem) { vkFreeMemory(device, sdfQuadVboMem, nullptr); sdfQuadVboMem = VK_NULL_HANDLE; }
    if (sdfQuadIbo) { vkDestroyBuffer(device, sdfQuadIbo, nullptr); sdfQuadIbo = VK_NULL_HANDLE; }
    if (sdfQuadIboMem) { vkFreeMemory(device, sdfQuadIboMem, nullptr); sdfQuadIboMem = VK_NULL_HANDLE; }

    auto destroyPipe = [&](VkPipeline& p){ if (p) { vkDestroyPipeline(device, p, nullptr); p = VK_NULL_HANDLE; } };
    destroyPipe(pipeBasic); destroyPipe(pipeText); destroyPipe(pipeMSDF);
    destroyPipe(pipeImage); destroyPipe(pipeLines); destroyPipe(pipeSDF);
    if (layoutNoTex) { vkDestroyPipelineLayout(device, layoutNoTex, nullptr); layoutNoTex = VK_NULL_HANDLE; }
    if (layoutTex)   { vkDestroyPipelineLayout(device, layoutTex, nullptr);   layoutTex = VK_NULL_HANDLE; }
    if (texSetLayout){ vkDestroyDescriptorSetLayout(device, texSetLayout, nullptr); texSetLayout = VK_NULL_HANDLE; }

    auto destroyShader = [&](VkShaderModule& m){ if (m) { vkDestroyShaderModule(device, m, nullptr); m = VK_NULL_HANDLE; } };
    destroyShader(vertModule); destroyShader(basicFrag); destroyShader(textFrag);
    destroyShader(msdfFrag); destroyShader(imageFrag);
    destroyShader(sdfVertModule); destroyShader(sdfFrag);

    // Standalone-owned objects.
    if (ownsDevice) {
        DestroySwapchain();
        for (int i = 0; i < kFramesInFlight; ++i) {
            if (frames[i].imageAvailable) vkDestroySemaphore(device, frames[i].imageAvailable, nullptr);
            if (frames[i].inFlight) vkDestroyFence(device, frames[i].inFlight, nullptr);
            frames[i] = FrameSync{};
        }
        if (commandPool) { vkDestroyCommandPool(device, commandPool, nullptr); commandPool = VK_NULL_HANDLE; }
    }
    if (uploadPool) { vkDestroyCommandPool(device, uploadPool, nullptr); uploadPool = VK_NULL_HANDLE; }
    if (ownsRenderPass && renderPass) { vkDestroyRenderPass(device, renderPass, nullptr); }
    renderPass = VK_NULL_HANDLE;
    ownsRenderPass = false;

    if (ownsDevice) {
        if (device)   { vkDestroyDevice(device, nullptr); device = VK_NULL_HANDLE; }
        if (surface)  { vkDestroySurfaceKHR(instance, surface, nullptr); surface = VK_NULL_HANDLE; }
        if (instance) { vkDestroyInstance(instance, nullptr); instance = VK_NULL_HANDLE; }
    } else {
        device = VK_NULL_HANDLE; instance = VK_NULL_HANDLE; // borrowed, don't destroy
    }
}

} // namespace FluentUI
