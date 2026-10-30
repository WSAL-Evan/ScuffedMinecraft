#include <wv/rendering/Renderer.h>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <wv/rendering/Camera.h>
#include <wv/Logger.h>
#include <wv/Util.h>
#include <map>

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

std::vector<const char*> requiredDeviceExtensions = { vk::KHRSwapchainExtensionName, vk::EXTMemoryBudgetExtensionName };

namespace WillowVox
{
    Renderer* Renderer::s_instance = nullptr;

    static vk::VertexInputBindingDescription GetVertexBindingDescription()
    {
        return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
    }

    static std::array<vk::VertexInputAttributeDescription, 2> GetVertexAttributeDescriptions()
    {
        return { {
            {.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
            {.location = 1, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, texCoord)}
        } };
    }

    struct UniformBufferObject
    {
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct PushConstants
    {
        glm::mat4 model;
    };

    struct ResourceCreationContext
    {
        vk::CommandPool commandPool;
        vk::CommandBuffer commandBuffer;
        vk::Fence fence;

        ResourceCreationContext(vk::CommandPool cmdp, vk::CommandBuffer cmdb, vk::Fence fence)
            : commandPool(cmdp), commandBuffer(cmdb), fence(fence) {}
    };

    class DeletionQueue
    {
    public:
        void Push(int frameIndex, std::function<void()> destroyFunction)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queues[frameIndex].push_back(std::move(destroyFunction));
        }

        void Delete(int frameIndex)
        {
            std::vector<std::function<void()>> resourcesToDelete;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                resourcesToDelete.swap(_queues[frameIndex]);
            }
            for (auto& fn : resourcesToDelete) fn();
        }

    private:
        std::mutex _mutex;
        std::vector<std::function<void()>> _queues[MAX_FRAMES_IN_FLIGHT];
    };

    struct Renderer::Impl
    {
        DeletionQueue deletionQueue;

        // Primary resources
        Window* window = nullptr;

        vk::Instance instance = nullptr;
        vk::PhysicalDevice physicalDevice = nullptr;
        vk::Device device = nullptr;

        VmaAllocator allocator;

        // Render variables
        std::mutex queueMutex;
        vk::Queue queue = nullptr;
        uint32_t queueIndex = 0;

        vk::SurfaceKHR surface = nullptr;

        vk::Extent2D swapChainExtent;
        vk::SurfaceFormatKHR swapChainSurfaceFormat;
        vk::SwapchainKHR swapChain = nullptr;
        std::vector<vk::Image> swapChainImages;
        std::vector<vk::ImageView> swapChainImageViews;

        vk::DescriptorSetLayout descriptorSetLayout = nullptr;
        vk::DescriptorPool descriptorPool = nullptr;
        std::vector<vk::DescriptorSet> descriptorSets;

        vk::CommandPool renderCommandPool = nullptr;
        std::vector<vk::CommandBuffer> renderCommandBuffers;

        std::vector<vk::Semaphore> presentCompleteSemaphores;
        std::vector<vk::Semaphore> renderFinishedSemaphores;
        std::vector<vk::Fence> drawFences;

        vk::Image depthImage = nullptr;
        VmaAllocation depthImageAlloc = nullptr;
        vk::ImageView depthImageView = nullptr;

        std::atomic<uint32_t> frameIndex = 0;
        uint32_t currentImageIndex = 0;

        vk::ClearValue clearColor = vk::ClearColorValue(0.015f, 0.015f, 0.015f, 1.0f);

        // Resource management variables
        std::shared_mutex resourceMapMutex;
        std::unordered_map<std::thread::id, ResourceCreationContext> resourceCreationContexts;

        // Global uniform buffer
        std::vector<vk::Buffer> uniformBuffers;
        std::vector<VmaAllocation> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        // Temporary global image
        vk::Image textureImage = nullptr;
        VmaAllocation textureImageAlloc = nullptr;
        vk::ImageView textureImageView = nullptr;
        vk::Sampler textureSampler = nullptr;

        // Primary resource functions
        void InitVulkan();
        void CreateSurface();
        void PickPhysicalDevice();
        bool IsDeviceSuitable(const vk::PhysicalDevice& device);
        void CreateLogicalDevice();
        void CreateVmaAllocator();

        // Render variable initialization
        vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;
        void CreateSwapChain();
        void CreateImageViews();
        void CreateDescriptorSetLayout();
        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateCommandPool();
        void CreateDepthResources();
        void CreateTextureImage();
        void CreateTextureImageView();
        void CreateTextureSampler();
        void CreateUniformBuffers();
        void CreateCommandBuffers();
        void CreateSyncObjects();

        void InitImGui();

        // Render functions
        void TransitionSwapChainImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask, vk::ImageAspectFlags imageAspectFlags);
        void StartRecordCommandBuffer(uint32_t imageIndex);
        void EndRecordCommandBuffer(uint32_t imageIndex);

        void CleanupSwapChain();
        void RecreateSwapChain();

        // Resource management functions
        ResourceCreationContext& GetThreadResourceCreationContext();
    };

    Renderer::Renderer(Window* window)
    {
        s_instance = this;

        LOG_INFO("Initializing renderer...");
        if (enableValidationLayers)
            LOG_INFO("Vulkan validation layers enabled");

        _impl = std::make_unique<Impl>();

        _impl->window = window;

        _impl->InitVulkan();
        _impl->CreateSurface();
        _impl->PickPhysicalDevice();
        _impl->CreateLogicalDevice();
        _impl->CreateVmaAllocator();
        _impl->CreateSwapChain();
        _impl->CreateImageViews();
        _impl->CreateDescriptorSetLayout();
        _impl->CreateCommandPool();
        _impl->CreateDepthResources();
        _impl->CreateTextureImage();
        _impl->CreateTextureImageView();
        _impl->CreateTextureSampler();
        _impl->CreateUniformBuffers();
        _impl->CreateDescriptorPool();
        _impl->CreateDescriptorSets();
        _impl->CreateCommandBuffers();
        _impl->CreateSyncObjects();

        _impl->InitImGui();

        LOG_SUCCESS("Successfully initialized renderer");
    }

    void Renderer::Impl::InitVulkan()
    {
        constexpr vk::ApplicationInfo appInfo{ .pApplicationName = "Scuffed Minecraft",
                                        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                        .pEngineName = "WillowVox Engine",
                                        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                        .apiVersion = vk::ApiVersion14 };

        // Get required layers
        std::vector<const char*> requiredLayers;
        if (enableValidationLayers)
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());

        // Check if required layers are supported by the Vulkan implementation
        auto layerProperties = vk::enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
            [&layerProperties](const auto& requiredLayer) {
                return std::ranges::none_of(layerProperties,
                    [requiredLayer](const auto& layerProperty) {
                        return strcmp(layerProperty.layerName, requiredLayer) == 0;
                    });
            });

        if (unsupportedLayerIt != requiredLayers.end())
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));

        // Get required extensions from GLFW
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // Check if required GLFW extensions are supported by the Vulkan implementation
        auto extensionProperties = vk::enumerateInstanceExtensionProperties();
        for (uint32_t i = 0; i < glfwExtensionCount; ++i)
        {
            if (std::ranges::none_of(extensionProperties,
                [glfwExtension = glfwExtensions[i]](const auto& extensionProperty)
                { return strcmp(extensionProperty.extensionName, glfwExtension) == 0; }))
            {
                throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
            }
        }

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions
        };

        instance = vk::createInstance(createInfo);
    }

    void Renderer::Impl::CreateSurface()
    {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(instance, static_cast<GLFWwindow*>(window->GetNativeWindow()), nullptr, &_surface) != 0)
            throw std::runtime_error("Failed to create window surface!");

        surface = _surface;

        LOG_INFO("Created window surface");
    }

    void Renderer::Impl::PickPhysicalDevice()
    {
        // Get all supported GPUs
        auto physicalDevices = instance.enumeratePhysicalDevices();
        if (physicalDevices.empty())
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");

        // Score the GPUs based on their features
        std::multimap<int, vk::PhysicalDevice> candidates;
        for (auto pd : physicalDevices)
        {
            if (!IsDeviceSuitable(pd)) continue;

            auto deviceProperties = pd.getProperties();
            auto deviceFeatures = pd.getFeatures();
            int score = 1;


            LOG_INFO("Found suitable GPU: %s", deviceProperties.deviceName.data());
            LOG_INFO("Max Allocation Count: %d", deviceProperties.limits.maxMemoryAllocationCount);

            // Score discrete GPUs higher than integrated ones
            if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                score += 1000;

            candidates.insert(std::make_pair(score, pd));
        }
        
        // Pick the best GPU
        if (!candidates.empty() && candidates.rbegin()->first > 0)
            physicalDevice = candidates.rbegin()->second;
        else
            throw std::runtime_error("Failed to find a suitable GPU!");

        LOG_INFO("Using '%s' for rendering", physicalDevice.getProperties().deviceName.data());
    }

    bool Renderer::Impl::IsDeviceSuitable(const vk::PhysicalDevice& device)
    {
        auto deviceProperties = device.getProperties();
        auto deviceFeatures = device.getFeatures();

        // Check to see if the device is suitable for the application
        if (deviceProperties.apiVersion < vk::ApiVersion13) // Minimum Vulkan 1.3 support
            return false;

        // Must support graphics commands
        auto queueFamilies = device.getQueueFamilyProperties();
        bool supportsGraphics = std::ranges::any_of(queueFamilies, [](const auto& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });
        if (!supportsGraphics) return false;

        // Must support required extensions
        auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtensions,
            [&availableDeviceExtensions](const auto& requiredDeviceExtension)
            {
                return std::ranges::any_of(availableDeviceExtensions,
                    [requiredDeviceExtension](const auto& availableDeviceExtension)
                    { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
            });
        if (!supportsAllRequiredExtensions) return false;

        // Must support features required by the application
        auto features = device.template getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
                                        features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                        features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
                                        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;
        if (!supportsRequiredFeatures) return false;

        return true;
    }

    void Renderer::Impl::CreateLogicalDevice()
    {
        // Queues to be created
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // Get the first queue family that supports both graphics and present for the render queue
        queueIndex = ~0;
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(qfpIndex, surface))
            {
                // Found a valid queue family
                queueIndex = qfpIndex;
                break;
            }
        }
        if (queueIndex == ~0)
            throw std::runtime_error("Could not find a queue for graphics and present!");

        // Device features to use
        vk::StructureChain< vk::PhysicalDeviceFeatures2,
                            vk::PhysicalDeviceVulkan11Features,
                            vk::PhysicalDeviceVulkan13Features,
                            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
                { .features = {.samplerAnisotropy = true } },
                { .shaderDrawParameters = true },
                { .synchronization2 = true, .dynamicRendering = true },
                { .extendedDynamicState = true }
        };

        // Create logical device
        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{ .queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority };
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
            .ppEnabledExtensionNames = requiredDeviceExtensions.data()
        };

        device = physicalDevice.createDevice(deviceCreateInfo);

        queue = device.getQueue(queueIndex, 0);

        LOG_INFO("Created logical device");
    }

    void Renderer::Impl::CreateVmaAllocator()
    {
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorCreateInfo.physicalDevice = physicalDevice;
        allocatorCreateInfo.device = device;
        allocatorCreateInfo.instance = instance;
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

        vmaCreateAllocator(&allocatorCreateInfo, &allocator);
    }

    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        assert(!availableFormats.empty());

        const auto formatIt = std::ranges::find_if(
            availableFormats,
            [](const auto& format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
        return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
    }

    vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
        return std::ranges::any_of(availablePresentModes,
            [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
            vk::PresentModeKHR::eMailbox :
            vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D Renderer::Impl::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        int width, height;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window->GetNativeWindow()), &width, &height);

        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    uint32_t ChooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities)
    {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
            minImageCount = surfaceCapabilities.maxImageCount;

        return minImageCount;
    }

    void Renderer::Impl::CreateSwapChain()
    {
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
        swapChainExtent = ChooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = ChooseSwapMinImageCount(surfaceCapabilities);

        auto availableFormats = physicalDevice.getSurfaceFormatsKHR(surface);
        swapChainSurfaceFormat = ChooseSwapSurfaceFormat(availableFormats);

        auto availablePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = surface,
            .minImageCount = minImageCount,
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = ChooseSwapPresentMode(availablePresentModes),
            .clipped = true
        };

        swapChain = device.createSwapchainKHR(swapChainCreateInfo);
        swapChainImages = device.getSwapchainImagesKHR(swapChain);

        LOG_INFO("Created swap chain");
    }

    static inline vk::ImageView CreateImageView(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags)
    {
        vk::ImageViewCreateInfo viewInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}
        };
        return device.createImageView(viewInfo);
    }

    void Renderer::Impl::CreateImageViews()
    {
        assert(swapChainImageViews.empty());

        swapChainImageViews.reserve(swapChainImages.size());
        for (auto& image : swapChainImages)
        {
            swapChainImageViews.push_back(CreateImageView(device, image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor));
        }

        LOG_INFO("Created %d image views", swapChainImageViews.size());
    }

    void Renderer::Impl::CreateDescriptorSetLayout()
    {
        std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
            {{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex },
             {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}}
        };

        vk::DescriptorSetLayoutCreateInfo layoutInfo{ .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};
        descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
    }

    static inline vk::Format FindSupportedFormat(vk::PhysicalDevice physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
    {
        for (const auto format : candidates)
        {
            vk::FormatProperties props = physicalDevice.getFormatProperties(format);
            if (((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
                ((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features)))
            {
                return format;
            }
        }

        throw std::runtime_error("Failed to find supported format!");
    }

    static inline vk::Format FindDepthFormat(vk::PhysicalDevice physicalDevice)
    {
        return FindSupportedFormat(physicalDevice, { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }

    void Renderer::Impl::CreateCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueIndex
        };

        renderCommandPool = device.createCommandPool(poolInfo);
        
        LOG_INFO("Created command pool");
    }

    static inline void BeginSingleTimeCommands(vk::Device device, ResourceCreationContext& context)
    {
        vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
        context.commandBuffer.begin(beginInfo);
    }

    static inline void EndSingleTimeCommands(vk::Device device, ResourceCreationContext& context, vk::Queue queue, std::mutex& queueMutex)
    {
        context.commandBuffer.end();

        vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &context.commandBuffer };
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            queue.submit(submitInfo, context.fence);
        }

        device.waitForFences(context.fence, vk::True, UINT64_MAX);
        device.resetFences(context.fence);
    }

    static inline uint32_t FindMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    static inline std::pair<vk::Image, VmaAllocation> CreateImage(VmaAllocator allocator, uint32_t width, uint32_t height,
        vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage)
    {
        vk::ImageCreateInfo imageInfo{
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = { width, height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive
        };

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VkImage image;
        VmaAllocation allocation;
        VkResult result = vmaCreateImage(allocator, reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo, &image, &allocation, nullptr);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Error creating image: %d", result);
            throw std::runtime_error("Error creating image!");
        }

        return { vk::Image(image), allocation };
    }

    static inline void TransitionImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
    {
        vk::ImageMemoryBarrier barrier{
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 }
        };

        vk::PipelineStageFlags sourceStage;
        vk::PipelineStageFlags destinationStage;

        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
    }

    enum class BufferAccessType
    {
        GpuOnly,
        CpuAccessible,
        Temporary
    };

    static inline std::pair<vk::Buffer, VmaAllocation> CreateBuffer(VmaAllocator allocator,
        vk::DeviceSize size, vk::BufferUsageFlags usage, BufferAccessType accessType, VmaAllocationInfo* resultAllocInfo = nullptr)
    {
        vk::BufferCreateInfo bufferInfo{ .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive };

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        switch (accessType)
        {
        case BufferAccessType::GpuOnly:
            break;
        case BufferAccessType::CpuAccessible:
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case BufferAccessType::Temporary:
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        }

        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo resultInfo;
        auto result = vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, &buffer, &allocation, &resultInfo);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Error creating buffer: %d", result);
            throw std::runtime_error("Error creating buffer!");
        }

        if (resultAllocInfo)
            *resultAllocInfo = resultInfo;
        return { vk::Buffer(buffer), allocation };
    }

    void Renderer::Impl::CreateDepthResources()
    {
        if (depthImageView)
            device.destroyImageView(depthImageView);
        if (depthImage)
            vmaDestroyImage(allocator, depthImage, depthImageAlloc);

        vk::Format depthFormat = FindDepthFormat(physicalDevice);

        std::tie(depthImage, depthImageAlloc) = CreateImage(allocator, swapChainExtent.width, swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment);
        depthImageView = CreateImageView(device, depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);

        LOG_INFO("Created depth resources");
    }

    void Renderer::Impl::CreateTextureImage()
    {
        stbi_set_flip_vertically_on_load(true);

        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("assets/textures/chunk_texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels)
            throw std::runtime_error("Failed to load texture image");

        VmaAllocationInfo resultInfo;
        auto [stagingBuffer, stagingBufferAlloc] =
            CreateBuffer(allocator, imageSize, vk::BufferUsageFlagBits::eTransferSrc, BufferAccessType::Temporary, &resultInfo);

        void* data;
        vmaMapMemory(allocator, stagingBufferAlloc, &data);
        memcpy(data, pixels, imageSize);
        vmaUnmapMemory(allocator, stagingBufferAlloc);
        vmaFlushAllocation(allocator, stagingBufferAlloc, 0, imageSize);

        stbi_image_free(pixels);

        std::tie(textureImage, textureImageAlloc) = CreateImage(allocator,
            texWidth, texHeight,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

        auto& creationContext = GetThreadResourceCreationContext();
        BeginSingleTimeCommands(device, creationContext);
        TransitionImageLayout(creationContext.commandBuffer, textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 }
        };

        creationContext.commandBuffer.copyBufferToImage(stagingBuffer, textureImage, vk::ImageLayout::eTransferDstOptimal, region);

        TransitionImageLayout(creationContext.commandBuffer, textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        EndSingleTimeCommands(device, creationContext, queue, queueMutex);

        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAlloc);

        LOG_INFO("Created texture image");
    }

    void Renderer::Impl::CreateTextureImageView()
    {
        textureImageView = CreateImageView(device, textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);

        LOG_INFO("Created texture image view");
    }

    void Renderer::Impl::CreateTextureSampler()
    {
        vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
        vk::SamplerCreateInfo samplerInfo{ 
            .magFilter = vk::Filter::eNearest, 
            .minFilter = vk::Filter::eNearest,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .anisotropyEnable = vk::False,
            .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
            .compareEnable = vk::False,
            .compareOp = vk::CompareOp::eAlways,
            .borderColor = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = vk::False
        };
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        textureSampler = device.createSampler(samplerInfo);

        LOG_INFO("Created texture sampler");
    }

    static inline void CopyBuffer(vk::Device device, ResourceCreationContext context, vk::Queue queue, std::mutex& queueMutex, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
    {
        BeginSingleTimeCommands(device, context);
        context.commandBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
        EndSingleTimeCommands(device, context, queue, queueMutex);
    }

    void Renderer::Impl::CreateUniformBuffers()
    {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

            VmaAllocationInfo allocInfo;
            auto [buffer, bufferMem] = CreateBuffer(allocator,
                bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, BufferAccessType::CpuAccessible, &allocInfo);
            uniformBuffers.push_back(buffer);
            uniformBuffersMemory.push_back(bufferMem);
            uniformBuffersMapped.emplace_back(allocInfo.pMappedData);
        }

        LOG_INFO("Created uniform buffers");
    }

    void Renderer::Impl::CreateDescriptorPool()
    {
        std::array<vk::DescriptorPoolSize, 2> poolSize{
            {{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = MAX_FRAMES_IN_FLIGHT},
             {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = MAX_FRAMES_IN_FLIGHT}}
        };
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
            .pPoolSizes = poolSize.data()
        };

        descriptorPool = device.createDescriptorPool(poolInfo);

        LOG_INFO("Created descriptor pool");
    }

    void Renderer::Impl::CreateDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data()
        };

        descriptorSets = device.allocateDescriptorSets(allocInfo);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorBufferInfo bufferInfo{ .buffer = uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject) };
            vk::DescriptorImageInfo imageInfo{ .sampler = textureSampler, .imageView = textureImageView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };

            std::array<vk::WriteDescriptorSet, 2> descriptorWrite{
                {{
                    .dstSet = descriptorSets[i],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &bufferInfo},
                {
                    .dstSet = descriptorSets[i],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo = &imageInfo
                }}
            };

            device.updateDescriptorSets(descriptorWrite, {});
        }

        LOG_INFO("Created descriptor sets");
    }

    void Renderer::Impl::CreateCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = renderCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT
        };

        renderCommandBuffers = device.allocateCommandBuffers(allocInfo);

        LOG_INFO("Created command buffer");
    }

    void Renderer::Impl::TransitionSwapChainImageLayout(
        vk::Image image,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout,
        vk::AccessFlags2 srcAccessMask,
        vk::AccessFlags2 dstAccessMask,
        vk::PipelineStageFlags2 srcStageMask,
        vk::PipelineStageFlags2 dstStageMask,
        vk::ImageAspectFlags imageAspectFlags)
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
            .image = image,
            .subresourceRange = {
                .aspectMask = imageAspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        vk::DependencyInfo dependencyInfo = {
            .dependencyFlags = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        };

        renderCommandBuffers[frameIndex.load()].pipelineBarrier2(dependencyInfo);
    }

    void Renderer::Impl::StartRecordCommandBuffer(uint32_t imageIndex)
    {
        auto& commandBuffer = renderCommandBuffers[frameIndex.load()];

        vk::CommandBufferBeginInfo beginInfo{};
        commandBuffer.begin(beginInfo);

        // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
        TransitionSwapChainImageLayout(
            swapChainImages[imageIndex],
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor
        );

        TransitionSwapChainImageLayout(
            depthImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::ImageAspectFlagBits::eDepth
        );

        vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearColor
        };

        vk::RenderingAttachmentInfo depthAttachmentInfo = {
            .imageView = depthImageView,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = clearDepth
        };

        vk::RenderingInfo renderingInfo = {
            .renderArea = { .offset = { 0, 0 }, .extent = swapChainExtent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo
        };

        commandBuffer.beginRendering(renderingInfo);
    }

    void Renderer::Impl::EndRecordCommandBuffer(uint32_t imageIndex)
    {
        auto& commandBuffer = renderCommandBuffers[frameIndex.load()];

        commandBuffer.endRendering();

        // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
        TransitionSwapChainImageLayout(
            swapChainImages[imageIndex],
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::ImageAspectFlagBits::eColor
        );

        commandBuffer.end();
    }

    void Renderer::Impl::CreateSyncObjects()
    {
        assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && drawFences.empty());

        for (size_t i = 0; i < swapChainImages.size(); i++)
            renderFinishedSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo()));

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {

            presentCompleteSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo()));
            drawFences.push_back(device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));
        }

        LOG_INFO("Sync objects created");
    }

    void Renderer::Impl::InitImGui()
    {
        ImGui::CreateContext();

        ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(window->GetNativeWindow()), true);

        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = queueIndex;
        initInfo.Queue = queue;
        initInfo.DescriptorPoolSize = 8;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = MAX_FRAMES_IN_FLIGHT;
        initInfo.ApiVersion = VK_API_VERSION_1_3;
        initInfo.UseDynamicRendering = true;

        ImGui_ImplVulkan_PipelineInfo pipelineInfo = {};
        pipelineInfo.PipelineRenderingCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = reinterpret_cast<const VkFormat*>(&swapChainSurfaceFormat.format),
            .depthAttachmentFormat = static_cast<VkFormat>(FindDepthFormat(physicalDevice)),
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
        };
        initInfo.PipelineInfoMain = pipelineInfo;

        ImGui_ImplVulkan_Init(&initInfo);

        LOG_INFO("ImGui Initialized");
    }

    bool Renderer::BeginRendering()
    {
        uint32_t newFrameIndex = (_impl->frameIndex.load() + 1) % MAX_FRAMES_IN_FLIGHT;
        auto fenceResult = _impl->device.waitForFences(_impl->drawFences[newFrameIndex], vk::True, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess)
            throw std::runtime_error("Failed to wait for fence!");

        // Delete all resources in deletion queue
        _impl->deletionQueue.Delete(newFrameIndex);
        _impl->frameIndex = newFrameIndex;

        auto [result, imageIndex] = _impl->device.acquireNextImageKHR(_impl->swapChain, UINT64_MAX, _impl->presentCompleteSemaphores[_impl->frameIndex.load()], nullptr);
        _impl->currentImageIndex = imageIndex;

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            _impl->RecreateSwapChain();
            return false;
        }
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
            throw std::runtime_error("Failed to acquire swap chain image!");
        }

        _impl->device.resetFences(_impl->drawFences[_impl->frameIndex.load()]);

        _impl->renderCommandBuffers[_impl->frameIndex.load()].reset();
        _impl->StartRecordCommandBuffer(imageIndex);

        return true;
    }

    void Renderer::BeginUI()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Renderer::EndRendering()
    {
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();

        {
            std::lock_guard<std::mutex> lock(_impl->queueMutex);
            ImGui_ImplVulkan_RenderDrawData(drawData, _impl->renderCommandBuffers[_impl->frameIndex.load()]);
        }

        _impl->EndRecordCommandBuffer(_impl->currentImageIndex);

        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &_impl->presentCompleteSemaphores[_impl->frameIndex.load()],
            .pWaitDstStageMask = &waitDestinationStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &_impl->renderCommandBuffers[_impl->frameIndex.load()],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &_impl->renderFinishedSemaphores[_impl->currentImageIndex]
        };

        const vk::PresentInfoKHR presentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &_impl->renderFinishedSemaphores[_impl->currentImageIndex],
            .swapchainCount = 1,
            .pSwapchains = &_impl->swapChain,
            .pImageIndices = &_impl->currentImageIndex
        };

        vk::Result result;
        {
            std::lock_guard<std::mutex> lock(_impl->queueMutex);
            _impl->queue.submit(submitInfo, _impl->drawFences[_impl->frameIndex.load()]);
            result = _impl->queue.presentKHR(presentInfoKHR);
        }
        if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR || _impl->window->windowResized)
        {
            _impl->window->windowResized = false;
            _impl->RecreateSwapChain();
        }
        else
            assert(result == vk::Result::eSuccess);
    }

    void Renderer::UpdateUniformBuffer(const Camera& camera)
    {
        UniformBufferObject ubo{};
        ubo.view = camera.GetViewMatrix();
        ubo.proj = camera.GetProjMatrix();
        ubo.proj[1][1] *= -1;

        memcpy(_impl->uniformBuffersMapped[_impl->frameIndex.load()], &ubo, sizeof(ubo));
        vmaFlushAllocation(_impl->allocator, _impl->uniformBuffersMemory[_impl->frameIndex.load()], 0, sizeof(ubo));
    }

    void Renderer::UpdatePushConstants(const Shader& shader, const glm::mat4& model)
    {
        PushConstants pushConstants{ .model = model };
        _impl->renderCommandBuffers[_impl->frameIndex.load()].pushConstants(
            *static_cast<vk::PipelineLayout*>(shader.GetPipelineLayout()),
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(PushConstants),
            &pushConstants
        );
    }

    void Renderer::SetClearColor(const glm::vec4& color)
    {
        _impl->clearColor = vk::ClearColorValue(color.r, color.g, color.b, color.a);
    }

    void Renderer::Impl::CleanupSwapChain()
    {
        for (auto imageView : swapChainImageViews)
            device.destroyImageView(imageView);
        swapChainImageViews.clear();
        device.destroySwapchainKHR(swapChain);
        swapChain = nullptr;
    }

    void Renderer::Impl::RecreateSwapChain()
    {
        auto size = window->GetSize();
        while (size.x == 0 || size.y == 0)
        {
            size = window->GetSize();
            glfwWaitEvents();
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            device.waitIdle();
        }

        CleanupSwapChain();

        CreateSwapChain();
        CreateImageViews();
        CreateDepthResources();
    }

    ResourceCreationContext& Renderer::Impl::GetThreadResourceCreationContext()
    {
        std::thread::id threadId = std::this_thread::get_id();
        // Get existing resources
        {
            std::shared_lock<std::shared_mutex> lock(resourceMapMutex);
            auto it = resourceCreationContexts.find(threadId);
            if (it != resourceCreationContexts.end())
                return it->second;
        }

        // Resources don't exist yet. Create the
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueIndex
        };
        vk::CommandPool commandPool = device.createCommandPool(poolInfo);

        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers(allocInfo);

        vk::Fence fence = device.createFence({});

        std::lock_guard<std::shared_mutex> createLock(resourceMapMutex);
        auto [it, result] = resourceCreationContexts.emplace(std::piecewise_construct, std::forward_as_tuple(threadId), std::forward_as_tuple(commandPool, commandBuffers[0], fence));
        return it->second;
    }

    Renderer::~Renderer()
    {
        if (!_impl) return;

        {
            std::lock_guard<std::mutex> lock(_impl->queueMutex);
            _impl->device.waitIdle();
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        // Destroy command resource creation contexts
        {
            std::lock_guard<std::shared_mutex> lock(_impl->resourceMapMutex);
            for (auto& [id, context] : _impl->resourceCreationContexts)
            {
                _impl->device.destroyFence(context.fence);
                _impl->device.destroyCommandPool(context.commandPool);
            }
        }

        // Destroy everything in deletion queue
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            _impl->deletionQueue.Delete(i);
        }

        // Destroy render resources
        for (auto semaphore : _impl->presentCompleteSemaphores)
            _impl->device.destroySemaphore(semaphore);
        for (auto semaphore : _impl->renderFinishedSemaphores)
            _impl->device.destroySemaphore(semaphore);
        for (auto fence : _impl->drawFences)
            _impl->device.destroyFence(fence);

        for (int i = 0; i < _impl->uniformBuffers.size(); i++)
        {
            vmaDestroyBuffer(_impl->allocator, _impl->uniformBuffers[i], _impl->uniformBuffersMemory[i]);
        }

        if (_impl->depthImageView)
            _impl->device.destroyImageView(_impl->depthImageView);
        if (_impl->depthImage)
            vmaDestroyImage(_impl->allocator, _impl->depthImage, _impl->depthImageAlloc);

        if (_impl->textureSampler)
            _impl->device.destroySampler(_impl->textureSampler);
        if (_impl->textureImageView)
            _impl->device.destroyImageView(_impl->textureImageView);
        if (_impl->textureImage)
            vmaDestroyImage(_impl->allocator, _impl->textureImage, _impl->textureImageAlloc);

        if (_impl->renderCommandPool)
            _impl->device.destroyCommandPool(_impl->renderCommandPool);

        if (_impl->descriptorPool)
            _impl->device.destroyDescriptorPool(_impl->descriptorPool);
        if (_impl->descriptorSetLayout)
            _impl->device.destroyDescriptorSetLayout(_impl->descriptorSetLayout);

        for (auto imageView : _impl->swapChainImageViews) 
            _impl->device.destroyImageView(imageView);
        if (_impl->swapChain)
            _impl->device.destroySwapchainKHR(_impl->swapChain);

        // Destroy primary resources
        if (_impl->allocator)
            vmaDestroyAllocator(_impl->allocator);

        if (_impl->device)
            _impl->device.destroy();

        if (_impl->surface)
            _impl->instance.destroySurfaceKHR(_impl->surface);
        if (_impl->instance)
            _impl->instance.destroy();

        LOG_INFO("Successfully cleaned up rendering resources");
    }

    static std::vector<vk::VertexInputAttributeDescription> ConvertVertexAttributesToVulkan(const std::vector<VertexAttribute>& attributes)
    {
        std::vector<vk::VertexInputAttributeDescription> vulkanAttributes;
        for (auto& a : attributes)
        {
            vk::Format format;
            switch (a.format)
            {
            case VertexAttributeType::FLOAT32:
                format = vk::Format::eR32Sfloat;
                break;
            case VertexAttributeType::FLOAT32_2:
                format = vk::Format::eR32G32Sfloat;
                break;
            case VertexAttributeType::FLOAT32_3:
                format = vk::Format::eR32G32B32Sfloat;
                break;
            case VertexAttributeType::FLOAT32_4:
                format = vk::Format::eR32G32B32A32Sfloat;
                break;

            case VertexAttributeType::INT32:
                format = vk::Format::eR32Sint;
                break;
            case VertexAttributeType::INT32_2:
                format = vk::Format::eR32G32Sint;
                break;
            case VertexAttributeType::INT32_3:
                format = vk::Format::eR32G32B32Sint;
                break;
            case VertexAttributeType::INT32_4:
                format = vk::Format::eR32G32B32A32Sint;
                break;

            case VertexAttributeType::UINT32:
                format = vk::Format::eR32Uint;
                break;
            case VertexAttributeType::UINT32_2:
                format = vk::Format::eR32G32Uint;
                break;
            case VertexAttributeType::UINT32_3:
                format = vk::Format::eR32G32B32Uint;
                break;
            case VertexAttributeType::UINT32_4:
                format = vk::Format::eR32G32B32A32Uint;
                break;

            case VertexAttributeType::INT16:
                format = vk::Format::eR16Sint;
                break;
            case VertexAttributeType::INT16_2:
                format = vk::Format::eR16G16Sint;
                break;
            case VertexAttributeType::INT16_3:
                format = vk::Format::eR16G16B16Sint;
                break;
            case VertexAttributeType::INT16_4:
                format = vk::Format::eR16G16B16A16Sint;
                break;

            case VertexAttributeType::UINT16:
                format = vk::Format::eR16Uint;
                break;
            case VertexAttributeType::UINT16_2:
                format = vk::Format::eR16G16Uint;
                break;
            case VertexAttributeType::UINT16_3:
                format = vk::Format::eR16G16B16Uint;
                break;
            case VertexAttributeType::UINT16_4:
                format = vk::Format::eR16G16B16A16Uint;
                break;

            case VertexAttributeType::INT8:
                format = vk::Format::eR8Sint;
                break;
            case VertexAttributeType::INT8_2:
                format = vk::Format::eR8G8Sint;
                break;
            case VertexAttributeType::INT8_3:
                format = vk::Format::eR8G8B8Sint;
                break;
            case VertexAttributeType::INT8_4:
                format = vk::Format::eR8G8B8A8Sint;
                break;

            case VertexAttributeType::UINT8:
                format = vk::Format::eR8Uint;
                break;
            case VertexAttributeType::UINT8_2:
                format = vk::Format::eR8G8Uint;
                break;
            case VertexAttributeType::UINT8_3:
                format = vk::Format::eR8G8B8Uint;
                break;
            case VertexAttributeType::UINT8_4:
                format = vk::Format::eR8G8B8A8Uint;
                break;

            default:
                format = vk::Format::eR32G32B32Sfloat;
                break;
            }

            vulkanAttributes.push_back({
                .location = a.location,
                .binding = 0,
                .format = format,
                .offset = a.offset
                });
        }

        return vulkanAttributes;
    }

    Shader* Renderer::CreateShader(const std::string& shaderPath, uint32_t vertexSize, const std::vector<VertexAttribute>& attributes)
    {
        // Create shader module
        auto shaderCode = ReadFile(shaderPath);

        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = shaderCode.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
        };

        vk::ShaderModule shaderModule = _impl->device.createShaderModule(createInfo);

        // Define shader stages
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = "vertMain"
        };
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName = "fragMain"
        };
        vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // Define vertex info
        vk::VertexInputBindingDescription bindingDescription = {
            .binding = 0,
            .stride = vertexSize,
            .inputRate = vk::VertexInputRate::eVertex
        };
        auto vertexAttributes = ConvertVertexAttributesToVulkan(attributes);
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
            .pVertexAttributeDescriptions = vertexAttributes.data()
        };

        // Common defines for all shaders
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList };

        std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data() };

        vk::PipelineViewportStateCreateInfo viewportState{ .viewportCount = 1, .scissorCount = 1 };

        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = vk::False
        };

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };

        vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencil{
            .depthTestEnable = vk::True,
            .depthWriteEnable = vk::True,
            .depthCompareOp = vk::CompareOp::eLess,
            .depthBoundsTestEnable = vk::False,
            .stencilTestEnable = vk::False
        };

        vk::PushConstantRange pushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(PushConstants)
        };

        // Create pipeline layout
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{ .setLayoutCount = 1, .pSetLayouts = &_impl->descriptorSetLayout, .pushConstantRangeCount = 1, .pPushConstantRanges = &pushConstantRange };
        vk::PipelineLayout pipelineLayout = _impl->device.createPipelineLayout(pipelineLayoutInfo);

        // Create pipeline
        vk::Format depthFormat = FindDepthFormat(_impl->physicalDevice);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
            {
                .stageCount = 2,
                .pStages = shaderStages,
                .pVertexInputState = &vertexInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depthStencil,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = pipelineLayout,
                .renderPass = nullptr
            },
            {
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &_impl->swapChainSurfaceFormat.format,
                .depthAttachmentFormat = depthFormat
            }
        };

        vk::Pipeline pipeline = _impl->device.createGraphicsPipeline(nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()).value;

        // Cleanup creation resources
        _impl->device.destroyShaderModule(shaderModule);

        LOG_INFO("Created shader from %s", shaderPath.c_str());

        return new Shader(&_impl->device, &pipelineLayout, &pipeline);
    }

    Mesh* Renderer::CreateMesh(const void* vertices, uint32_t verticesSize, const uint32_t* indices, uint32_t indexCount)
    {
        if (verticesSize == 0 || indexCount == 0)
        {
            return new Mesh(this, nullptr, nullptr, nullptr, nullptr, 0);
        }

        auto& creationContext = _impl->GetThreadResourceCreationContext();

        // Create vertex buffer
        vk::DeviceSize vertexBufferSize = verticesSize;

        auto [vertexStagingBufer, vertexStagingBufferAlloc] =
            CreateBuffer(_impl->allocator, vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, BufferAccessType::Temporary);

        void* vertexDataStaging;
        vmaMapMemory(_impl->allocator, vertexStagingBufferAlloc, &vertexDataStaging);
        memcpy(vertexDataStaging, vertices, vertexBufferSize);
        vmaUnmapMemory(_impl->allocator, vertexStagingBufferAlloc);
        vmaFlushAllocation(_impl->allocator, vertexStagingBufferAlloc, 0, vertexBufferSize);

        auto [vertexBuffer, vertexBufferAlloc] =
            CreateBuffer(_impl->allocator, vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, BufferAccessType::GpuOnly);

        CopyBuffer(_impl->device, creationContext, _impl->queue, _impl->queueMutex, vertexStagingBufer, vertexBuffer, vertexBufferSize);

        vmaDestroyBuffer(_impl->allocator, vertexStagingBufer, vertexStagingBufferAlloc);

        // Create index buffer
        vk::DeviceSize indexBufferSize = sizeof(indices[0]) * indexCount;

        auto [indexStagingBufer, indexStagingBufferAlloc] =
            CreateBuffer(_impl->allocator, indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, BufferAccessType::Temporary);

        void* indexDataStaging;
        vmaMapMemory(_impl->allocator, indexStagingBufferAlloc, &indexDataStaging);
        memcpy(indexDataStaging, indices, indexBufferSize);
        vmaUnmapMemory(_impl->allocator, indexStagingBufferAlloc);
        vmaFlushAllocation(_impl->allocator, indexStagingBufferAlloc, 0, indexBufferSize);

        auto [indexBuffer, indexBufferAlloc] =
            CreateBuffer(_impl->allocator, indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, BufferAccessType::GpuOnly);

        CopyBuffer(_impl->device, creationContext, _impl->queue, _impl->queueMutex, indexStagingBufer, indexBuffer, indexBufferSize);

        vmaDestroyBuffer(_impl->allocator, indexStagingBufer, indexStagingBufferAlloc);

        // Create mesh
        return new Mesh(this, &vertexBuffer, &vertexBufferAlloc, &indexBuffer, &indexBufferAlloc, indexCount);
    }

    void Renderer::DestroyMesh(Mesh* mesh)
    {
        vk::Buffer vertexBuffer = *static_cast<vk::Buffer*>(mesh->GetVertexBuffer());
        VmaAllocation vertexBufferAlloc = *static_cast<VmaAllocation*>(mesh->GetVertexBufferAlloc());
        vk::Buffer indexBuffer = *static_cast<vk::Buffer*>(mesh->GetIndexBuffer());
        VmaAllocation indexBufferAlloc = *static_cast<VmaAllocation*>(mesh->GetIndexBufferAlloc());

        _impl->deletionQueue.Push(_impl->frameIndex.load(), [allocator = _impl->allocator, 
            vertexBuffer = vertexBuffer, vertexBufferAlloc = vertexBufferAlloc,
            indexBuffer = indexBuffer, indexBufferAlloc = indexBufferAlloc] {
                vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAlloc);
                vmaDestroyBuffer(allocator, indexBuffer, indexBufferAlloc);
                //LOG_INFO("Destroyed mesh");
        });
    }

    void Renderer::RenderMesh(const Shader& shader, const Mesh& mesh)
    {
        if (mesh.GetIndexCount() == 0) return;

        auto& commandBuffer = _impl->renderCommandBuffers[_impl->frameIndex.load()];

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *static_cast<vk::Pipeline*>(shader.GetPipeline()));

        commandBuffer.bindVertexBuffers(0, *static_cast<vk::Buffer*>(mesh.GetVertexBuffer()), {0});
        commandBuffer.bindIndexBuffer(*static_cast<vk::Buffer*>(mesh.GetIndexBuffer()), 0, vk::IndexType::eUint32);

        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(_impl->swapChainExtent.width), static_cast<float>(_impl->swapChainExtent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _impl->swapChainExtent));

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *static_cast<vk::PipelineLayout*>(shader.GetPipelineLayout()), 0, _impl->descriptorSets[_impl->frameIndex.load()], nullptr);
        commandBuffer.drawIndexed(mesh.GetIndexCount(), 1, 0, 0, 0);
    }

    void Renderer::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(_impl->queueMutex);
            _impl->device.waitIdle();
        }
    }
}