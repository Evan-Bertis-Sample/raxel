/*
 *  playground.c
 *
 *  To compile (example on Linux):
 *    cc minimal_vulkan_compute.c -o minimal_vulkan_compute \
 *       -lglfw -lvulkan -lm
 *
 *  This code is *not* production-ready and omits error checks, 
 *  validations, and many details for brevity.
 *
 *  Make sure to have:
 *    - compute.spv
 *    - vert.spv
 *    - frag.spv
 *  in the working directory or adjust the file paths accordingly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

// ---------- Constants ----------
#define WIDTH  800
#define HEIGHT 600

// Helper macro to check VkResult (you should handle errors properly!)
#define VK_CHECK(x)                                                       \
    do {                                                                  \
        VkResult err = x;                                                \
        if (err != VK_SUCCESS) {                                         \
            fprintf(stderr, "Detected Vulkan error: %d\n", err);          \
            exit(-1);                                                    \
        }                                                                 \
    } while (0)

// ---------- Global Variables (for brevity) ----------
static GLFWwindow*       g_window;
static VkInstance        g_instance;
static VkPhysicalDevice  g_physicalDevice;
static VkDevice          g_device;
static VkQueue           g_graphicsQueue;
static VkQueue           g_computeQueue;
static uint32_t          g_graphicsQueueFamilyIndex;
static uint32_t          g_computeQueueFamilyIndex;

static VkSurfaceKHR      g_surface;
static VkSwapchainKHR    g_swapchain;
static VkFormat          g_swapchainImageFormat;
static VkExtent2D        g_swapchainExtent;
static VkImageView*      g_swapchainImageViews;
static uint32_t          g_swapchainImageCount;

static VkCommandPool     g_commandPoolCompute;
static VkCommandPool     g_commandPoolGraphics;

static VkCommandBuffer   g_computeCmdBuf;
static VkCommandBuffer*  g_graphicsCmdBufs; // array per swapchain image

// Synchronization
static VkSemaphore       g_imageAvailableSemaphore;
static VkSemaphore       g_renderFinishedSemaphore;
static VkFence*          g_inFlightFences;

// Compute pipeline
static VkPipelineLayout  g_computePipelineLayout;
static VkPipeline        g_computePipeline;
static VkDescriptorSet   g_computeDescriptorSet;
static VkDescriptorSetLayout g_computeDescSetLayout;

// Graphics pipeline
static VkPipelineLayout  g_graphicsPipelineLayout;
static VkPipeline        g_graphicsPipeline;
static VkDescriptorSet   g_graphicsDescriptorSet;
static VkDescriptorSetLayout g_graphicsDescSetLayout;

// The storage image that compute shader writes into
static VkImage           g_storageImage;
static VkDeviceMemory    g_storageImageMemory;
static VkImageView       g_storageImageView;

// Descriptor pool for both compute & graphics
static VkDescriptorPool  g_descriptorPool;


// --------------- Forward Declarations ---------------
void initWindow(void);
void initVulkan(void);
void mainLoop(void);
void cleanup(void);

// Some sub-steps
void createInstance(void);
void pickPhysicalDevice(void);
void createLogicalDevice(void);
void createSurface(void);
void createSwapchain(void);
void createCommandPools(void);
void createComputeResources(void);
void createComputePipeline(void);
void recordComputeCommandBuffer(void);
void createGraphicsPipeline(void);
void createFrameResources(void);
void recordGraphicsCommandBuffers(void);

// The entry point
int main(void)
{
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
    return 0;
}

// --------------- Implementation ---------------

void initWindow(void)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    g_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Compute Example", NULL, NULL);
}

void initVulkan(void)
{
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createCommandPools();

    // Create descriptor pool for both compute & graphics
    {
        // We'll just create a single pool big enough for a few descriptors
        VkDescriptorPoolSize poolSizes[2];
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;   // for compute
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // for graphics
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo = {0};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = 2;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes    = poolSizes;

        VK_CHECK(vkCreateDescriptorPool(g_device, &poolInfo, NULL, &g_descriptorPool));
    }

    createComputeResources();
    createComputePipeline();
    recordComputeCommandBuffer();

    createGraphicsPipeline();
    createFrameResources();
    recordGraphicsCommandBuffers();
}

void mainLoop(void)
{
    while (!glfwWindowShouldClose(g_window))
    {
        glfwPollEvents();
        
        // 1. Submit compute
        {
            VkSubmitInfo submitInfo = {0};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            // Compute doesn't depend on any semaphores in this simplistic case
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers    = &g_computeCmdBuf;

            vkQueueSubmit(g_computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(g_computeQueue); 
        }

        // 2. Acquire swapchain image
        uint32_t imageIndex;
        vkAcquireNextImageKHR(
            g_device, 
            g_swapchain, 
            UINT64_MAX, 
            g_imageAvailableSemaphore, 
            VK_NULL_HANDLE, 
            &imageIndex
        );

        // 3. Submit graphics
        {
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            VkSubmitInfo submitInfo = {0};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            submitInfo.waitSemaphoreCount   = 1;
            submitInfo.pWaitSemaphores      = &g_imageAvailableSemaphore;
            submitInfo.pWaitDstStageMask    = waitStages;
            submitInfo.commandBufferCount   = 1;
            submitInfo.pCommandBuffers      = &g_graphicsCmdBufs[imageIndex];

            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores    = &g_renderFinishedSemaphore;

            vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        }

        // 4. Present
        {
            VkPresentInfoKHR presentInfo = {0};
            presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores    = &g_renderFinishedSemaphore;
            presentInfo.swapchainCount     = 1;
            presentInfo.pSwapchains        = &g_swapchain;
            presentInfo.pImageIndices      = &imageIndex;

            vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
            vkQueueWaitIdle(g_graphicsQueue);
        }
    }

    vkDeviceWaitIdle(g_device);
}

void cleanup(void)
{
    // Wait for device idle before cleanup
    vkDeviceWaitIdle(g_device);

    // Destroy our storage image
    if (g_storageImageView) vkDestroyImageView(g_device, g_storageImageView, NULL);
    if (g_storageImage) vkDestroyImage(g_device, g_storageImage, NULL);
    if (g_storageImageMemory) vkFreeMemory(g_device, g_storageImageMemory, NULL);

    // Destroy pipelines, layouts
    vkDestroyPipeline(g_device, g_computePipeline, NULL);
    vkDestroyPipelineLayout(g_device, g_computePipelineLayout, NULL);
    vkDestroyPipeline(g_device, g_graphicsPipeline, NULL);
    vkDestroyPipelineLayout(g_device, g_graphicsPipelineLayout, NULL);

    // Descriptor sets are implicitly freed with the descriptor pool
    if (g_descriptorPool) {
        vkDestroyDescriptorPool(g_device, g_descriptorPool, NULL);
    }

    // Destroy command pools
    if (g_commandPoolCompute)  vkDestroyCommandPool(g_device, g_commandPoolCompute, NULL);
    if (g_commandPoolGraphics) vkDestroyCommandPool(g_device, g_commandPoolGraphics, NULL);

    // Swapchain image views
    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        vkDestroyImageView(g_device, g_swapchainImageViews[i], NULL);
    }
    free(g_swapchainImageViews);

    vkDestroySwapchainKHR(g_device, g_swapchain, NULL);
    vkDestroyDevice(g_device, NULL);
    vkDestroySurfaceKHR(g_instance, g_surface, NULL);
    vkDestroyInstance(g_instance, NULL);
    glfwDestroyWindow(g_window);
    glfwTerminate();
}

// --------------- Sub-steps (Skeletal Implementations) ---------------

void createInstance(void)
{
    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Simple Compute Vulkan";
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Extensions for GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    createInfo.enabledExtensionCount   = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    VK_CHECK(vkCreateInstance(&createInfo, NULL, &g_instance));
}

void createSurface(void)
{
    // Let GLFW create the VkSurfaceKHR
    VK_CHECK(glfwCreateWindowSurface(g_instance, g_window, NULL, &g_surface));
}

void pickPhysicalDevice(void)
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_instance, &deviceCount, NULL);
    if (deviceCount == 0) {
        fprintf(stderr, "Failed to find GPUs with Vulkan support!\n");
        exit(1);
    }
    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    vkEnumeratePhysicalDevices(g_instance, &deviceCount, devices);

    // For simplicity, just pick the first device that can do compute & graphics
    for (uint32_t i = 0; i < deviceCount; i++) {
        g_physicalDevice = devices[i];
        // Normally you'd check features, memory, queue families, etc.
        // We'll just break here.
        break;
    }
    free(devices);

    // Query queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties* queueFamilies = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, queueFamilies);

    g_graphicsQueueFamilyIndex = 0;
    g_computeQueueFamilyIndex  = 0;

    // Find indices (very naive approach)
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && 
            (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            g_graphicsQueueFamilyIndex = i;
            g_computeQueueFamilyIndex  = i;
            break;
        }
    }

    free(queueFamilies);
}

void createLogicalDevice(void)
{
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[1] = {0};
    queueCreateInfos[0].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[0].queueFamilyIndex = g_graphicsQueueFamilyIndex; // same for compute in this example
    queueCreateInfos[0].queueCount       = 1;
    queueCreateInfos[0].pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo createInfo = {0};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = 1;
    createInfo.pQueueCreateInfos       = queueCreateInfos;

    // Enable swapchain extension
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    VK_CHECK(vkCreateDevice(g_physicalDevice, &createInfo, NULL, &g_device));

    vkGetDeviceQueue(g_device, g_graphicsQueueFamilyIndex, 0, &g_graphicsQueue);
    vkGetDeviceQueue(g_device, g_computeQueueFamilyIndex, 0, &g_computeQueue);
}

void createSwapchain(void)
{
    // Normally you'd query surface formats, choose one, etc. We'll guess something like:
    g_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    g_swapchainExtent.width  = WIDTH;
    g_swapchainExtent.height = HEIGHT;

    // We create a basic swapchain with triple buffering
    VkSwapchainCreateInfoKHR createInfo = {0};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = g_surface;
    createInfo.minImageCount    = 2;
    createInfo.imageFormat      = g_swapchainImageFormat;
    createInfo.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent      = g_swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped          = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(g_device, &createInfo, NULL, &g_swapchain));

    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchainImageCount, NULL);
    VkImage* swapchainImages = malloc(sizeof(VkImage) * g_swapchainImageCount);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchainImageCount, swapchainImages);

    g_swapchainImageViews = malloc(sizeof(VkImageView) * g_swapchainImageCount);

    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo = {0};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = swapchainImages[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = g_swapchainImageFormat;
        viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VK_CHECK(vkCreateImageView(g_device, &viewInfo, NULL, &g_swapchainImageViews[i]));
    }

    free(swapchainImages);
}

void createCommandPools(void)
{
    // Compute command pool
    {
        VkCommandPoolCreateInfo poolInfo = {0};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = g_computeQueueFamilyIndex;
        poolInfo.flags            = 0;

        VK_CHECK(vkCreateCommandPool(g_device, &poolInfo, NULL, &g_commandPoolCompute));
    }
    // Graphics command pool
    {
        VkCommandPoolCreateInfo poolInfo = {0};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = g_graphicsQueueFamilyIndex;
        poolInfo.flags            = 0;

        VK_CHECK(vkCreateCommandPool(g_device, &poolInfo, NULL, &g_commandPoolGraphics));
    }
}

void createComputeResources(void)
{
    // We'll create a 2D image the same size as the swapchain for the compute shader to write into

    // 1. Create the VkImage
    {
        VkImageCreateInfo imageInfo = {0};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT; // for storage
        imageInfo.extent.width  = WIDTH;
        imageInfo.extent.height = HEIGHT;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vkCreateImage(g_device, &imageInfo, NULL, &g_storageImage));
    }

    // 2. Allocate memory
    {
        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(g_device, g_storageImage, &memReq);

        VkMemoryAllocateInfo allocInfo = {0};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReq.size;

        // For brevity, pick the first memory type that is DEVICE_LOCAL
        // In real code, you'd query and match properly
        allocInfo.memoryTypeIndex = 0; // This is a placeholder!

        VK_CHECK(vkAllocateMemory(g_device, &allocInfo, NULL, &g_storageImageMemory));
        VK_CHECK(vkBindImageMemory(g_device, g_storageImage, g_storageImageMemory, 0));
    }

    // 3. Create image view
    {
        VkImageViewCreateInfo viewInfo = {0};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = g_storageImage;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VK_CHECK(vkCreateImageView(g_device, &viewInfo, NULL, &g_storageImageView));
    }
}

static VkShaderModule loadShaderModule(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char* code = malloc(size);
    fread(code, 1, size, file);
    fclose(file);

    VkShaderModuleCreateInfo createInfo = {0};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode    = (uint32_t*)code;

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(g_device, &createInfo, NULL, &module));

    free(code);
    return module;
}

void createComputePipeline(void)
{
    // Descriptor set layout for compute: single binding for storage image
    {
        VkDescriptorSetLayoutBinding binding = {0};
        binding.binding            = 0;
        binding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        binding.descriptorCount    = 1;
        binding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &binding;

        VK_CHECK(vkCreateDescriptorSetLayout(g_device, &layoutInfo, NULL, &g_computeDescSetLayout));
    }

    // Allocate descriptor set
    {
        VkDescriptorSetAllocateInfo allocInfo = {0};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = g_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &g_computeDescSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(g_device, &allocInfo, &g_computeDescriptorSet));
    }

    // Update descriptor set to point to our storage image
    {
        VkDescriptorImageInfo imageInfo = {0};
        imageInfo.imageView   = g_storageImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet write = {0};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = g_computeDescriptorSet;
        write.dstBinding      = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(g_device, 1, &write, 0, NULL);
    }

    // Create pipeline layout
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &g_computeDescSetLayout;

        VK_CHECK(vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, NULL, &g_computePipelineLayout));
    }

    // Create compute pipeline
    {
        VkShaderModule compShaderModule = loadShaderModule("compute.spv");

        VkPipelineShaderStageCreateInfo stageInfo = {0};
        stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = compShaderModule;
        stageInfo.pName  = "main";

        VkComputePipelineCreateInfo pipelineInfo = {0};
        pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage  = stageInfo;
        pipelineInfo.layout = g_computePipelineLayout;

        VK_CHECK(vkCreateComputePipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &g_computePipeline));

        vkDestroyShaderModule(g_device, compShaderModule, NULL);
    }
}

void recordComputeCommandBuffer(void)
{
    // Allocate a single command buffer
    {
        VkCommandBufferAllocateInfo allocInfo = {0};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = g_commandPoolCompute;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(g_device, &allocInfo, &g_computeCmdBuf));
    }

    // Record commands
    {
        VkCommandBufferBeginInfo beginInfo = {0};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(g_computeCmdBuf, &beginInfo);

        // We need the image to be in general layout for writing
        VkImageMemoryBarrier barrier = {0};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = g_storageImage;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        vkCmdPipelineBarrier(
            g_computeCmdBuf,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );

        vkCmdBindPipeline(g_computeCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, g_computePipeline);
        vkCmdBindDescriptorSets(g_computeCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                g_computePipelineLayout, 0, 1, &g_computeDescriptorSet, 0, NULL);

        // Dispatch enough groups for the whole image
        uint32_t groupSizeX = (WIDTH + 16 - 1) / 16;
        uint32_t groupSizeY = (HEIGHT + 16 - 1) / 16;
        vkCmdDispatch(g_computeCmdBuf, groupSizeX, groupSizeY, 1);

        vkEndCommandBuffer(g_computeCmdBuf);
    }
}

void createGraphicsPipeline(void)
{
    // Descriptor set layout for graphics: single combined image sampler
    {
        VkDescriptorSetLayoutBinding binding = {0};
        binding.binding            = 0;
        binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount    = 1;
        binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &binding;

        VK_CHECK(vkCreateDescriptorSetLayout(g_device, &layoutInfo, NULL, &g_graphicsDescSetLayout));
    }

    // Allocate descriptor set for graphics
    {
        VkDescriptorSetAllocateInfo allocInfo = {0};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = g_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &g_graphicsDescSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(g_device, &allocInfo, &g_graphicsDescriptorSet));
    }

    // Create a sampler
    VkSampler sampler;
    {
        VkSamplerCreateInfo samplerInfo = {0};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_NEAREST;
        samplerInfo.minFilter    = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        VK_CHECK(vkCreateSampler(g_device, &samplerInfo, NULL, &sampler));
    }

    // Update descriptor to use g_storageImageView + sampler
    {
        VkDescriptorImageInfo imageInfo = {0};
        imageInfo.sampler     = sampler;
        imageInfo.imageView   = g_storageImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet write = {0};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = g_graphicsDescriptorSet;
        write.dstBinding      = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(g_device, 1, &write, 0, NULL);
    }

    // Create pipeline layout
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &g_graphicsDescSetLayout;

        VK_CHECK(vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, NULL, &g_graphicsPipelineLayout));
    }

    // Create render pass (very minimal)
    VkRenderPass renderPass;
    {
        VkAttachmentDescription colorAttachment = {0};
        colorAttachment.format         = g_swapchainImageFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = {0};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {0};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = {0};
        renderPassInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount  = 1;
        renderPassInfo.pAttachments     = &colorAttachment;
        renderPassInfo.subpassCount     = 1;
        renderPassInfo.pSubpasses       = &subpass;

        VK_CHECK(vkCreateRenderPass(g_device, &renderPassInfo, NULL, &renderPass));
    }

    // Create pipeline (vertex + fragment)
    {
        VkShaderModule vertShaderModule = loadShaderModule("vert.spv");
        VkShaderModule fragShaderModule = loadShaderModule("frag.spv");

        VkPipelineShaderStageCreateInfo vertStageInfo = {0};
        vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertShaderModule;
        vertStageInfo.pName  = "main";

        VkPipelineShaderStageCreateInfo fragStageInfo = {0};
        fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragShaderModule;
        fragStageInfo.pName  = "main";

        VkPipelineShaderStageCreateInfo stages[] = {vertStageInfo, fragStageInfo};

        // Vertex input (none in this case, full-screen triangle)
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Viewport / scissor
        VkViewport viewport = {0};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = (float)WIDTH;
        viewport.height   = (float)HEIGHT;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {0};
        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = WIDTH;
        scissor.extent.height = HEIGHT;

        VkPipelineViewportStateCreateInfo viewportState = {0};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer = {0};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode    = VK_CULL_MODE_NONE;
        rasterizer.lineWidth   = 1.0f;

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling = {0};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Color blend
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending = {0};
        colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount   = 1;
        colorBlending.pAttachments      = &colorBlendAttachment;

        VkGraphicsPipelineCreateInfo pipelineInfo = {0};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages            = stages;
        pipelineInfo.pVertexInputState  = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState= &inputAssembly;
        pipelineInfo.pViewportState     = &viewportState;
        pipelineInfo.pRasterizationState= &rasterizer;
        pipelineInfo.pMultisampleState  = &multisampling;
        pipelineInfo.pColorBlendState   = &colorBlending;
        pipelineInfo.layout             = g_graphicsPipelineLayout;
        pipelineInfo.renderPass         = renderPass;
        pipelineInfo.subpass            = 0;

        VK_CHECK(vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &g_graphicsPipeline));

        vkDestroyShaderModule(g_device, vertShaderModule, NULL);
        vkDestroyShaderModule(g_device, fragShaderModule, NULL);
    }

    // We don't keep the render pass around for each frame in this minimal example,
    // but normally you'd store it. Let's store it in a local static if you want.
    // For simplicity, let's just store it in a local static here:
    // (In advanced code, you might keep a global or pass around to each frame.)
    static VkRenderPass s_renderPass;
    s_renderPass = renderPass;

    // We can store the renderPass in the pipeline layout or a global, but let's
    // keep it in mind for the command buffer recording.
}

void createFrameResources(void)
{
    // Create semaphores
    {
        VkSemaphoreCreateInfo semInfo = {0};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(g_device, &semInfo, NULL, &g_imageAvailableSemaphore));
        VK_CHECK(vkCreateSemaphore(g_device, &semInfo, NULL, &g_renderFinishedSemaphore));
    }

    // Create a fence for each swapchain image
    g_inFlightFences = malloc(sizeof(VkFence) * g_swapchainImageCount);
    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VkFenceCreateInfo fenceInfo = {0};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(g_device, &fenceInfo, NULL, &g_inFlightFences[i]));
    }

    // Allocate command buffers for graphics
    g_graphicsCmdBufs = malloc(sizeof(VkCommandBuffer) * g_swapchainImageCount);

    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = g_commandPoolGraphics;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = g_swapchainImageCount;

    VK_CHECK(vkAllocateCommandBuffers(g_device, &allocInfo, g_graphicsCmdBufs));
}

void recordGraphicsCommandBuffers(void)
{
    // We'll create a simple framebuffer for each swapchain image
    // Then record a simple render pass which samples from our compute image
    for (uint32_t i = 0; i < g_swapchainImageCount; i++)
    {
        // For brevity, let's create a temporary render pass and framebuffer each time
        // (In reality, you'd reuse a single render pass object and create framebuffers up front.)

        // We do have the same renderPass from createGraphicsPipeline. 
        // Let's retrieve it from g_graphicsPipeline if we stored it somewhere. 
        // But we only stored it in a local var. Let's do a minimal approach:

        // Recreate the same render pass:
        VkAttachmentDescription colorAttachment = {0};
        colorAttachment.format         = g_swapchainImageFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = {0};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {0};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = {0};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments    = &colorAttachment;
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;

        VkRenderPass renderPass;
        VK_CHECK(vkCreateRenderPass(g_device, &renderPassInfo, NULL, &renderPass));

        // Framebuffer for this swapchain image
        VkFramebuffer framebuffer;
        {
            VkImageView attachments[] = { g_swapchainImageViews[i] };
            VkFramebufferCreateInfo fbInfo = {0};
            fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass      = renderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments    = attachments;
            fbInfo.width           = g_swapchainExtent.width;
            fbInfo.height          = g_swapchainExtent.height;
            fbInfo.layers          = 1;

            VK_CHECK(vkCreateFramebuffer(g_device, &fbInfo, NULL, &framebuffer));
        }

        VkCommandBufferBeginInfo beginInfo = {0};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        VK_CHECK(vkBeginCommandBuffer(g_graphicsCmdBufs[i], &beginInfo));

        // Begin render pass
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo rpBegin = {0};
        rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass        = renderPass;
        rpBegin.framebuffer       = framebuffer;
        rpBegin.renderArea.offset = (VkOffset2D){0,0};
        rpBegin.renderArea.extent = g_swapchainExtent;
        rpBegin.clearValueCount   = 1;
        rpBegin.pClearValues      = &clearColor;

        vkCmdBeginRenderPass(g_graphicsCmdBufs[i], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(g_graphicsCmdBufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphicsPipeline);
        vkCmdBindDescriptorSets(g_graphicsCmdBufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                g_graphicsPipelineLayout, 0, 1, &g_graphicsDescriptorSet, 0, NULL);

        vkCmdDraw(g_graphicsCmdBufs[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(g_graphicsCmdBufs[i]);

        VK_CHECK(vkEndCommandBuffer(g_graphicsCmdBufs[i]));

        // We destroy the framebuffer and render pass after recording
        // (In a real application, you'd keep them around or re-create them at swapchain recreation)
        vkDestroyFramebuffer(g_device, framebuffer, NULL);
        vkDestroyRenderPass(g_device, renderPass, NULL);
    }
}
