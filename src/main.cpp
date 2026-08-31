#include "utils.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <print>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

constexpr u32 WIDTH                = 800;
constexpr u32 HEIGHT               = 600;
constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

namespace kronk
{
struct ShaderData
{
   glm::mat4 projection;
   glm::mat4 view;
   glm::mat4 model[3];
   glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
   uint32_t  selected{1};
};

struct ShaderDataBuffer
{
   VkBuffer          buffer{};
   VmaAllocation     allocation{};
   VmaAllocationInfo allocationInfo{};
   VkDeviceAddress   deviceAddress{};
};

struct VkContext
{
   VkDevice                                           device{};
   VkPhysicalDevice                                   chosenGPU{};
   VkQueue                                            graphicsQueue{};
   u32                                                graphicsQueueFamily{};
   VmaAllocator                                       allocator{};
   VkSurfaceKHR                                       surface{};
   VkSurfaceCapabilitiesKHR                           surfaceCaps{};
   VkSwapchainKHR                                     swapchain{};
   std::vector<VkImage>                               swapchainImages{};
   std::vector<VkImageView>                           swapchainViews{};
   VkImage                                            depthImage{};
   VmaAllocation                                      depthImageAllocation{};
   VkImageView                                        depthImageView{};
   VkBuffer                                           vBuffer{};
   VmaAllocation                                      vBufferAllocation{};
   std::array<ShaderDataBuffer, MAX_FRAMES_IN_FLIGHT> shaderDataBuffers;
   std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT>  commandBuffers;
};

constexpr void Chk(VkResult result)
{
   if (result != VK_SUCCESS)
   {
      std::println("Vulkan call returned an error {}", static_cast<u32>(result));
      std::exit(result);
   }
}

VkContext Init(SDL_Window *window)
{
   u32 sdlExtensionCount = 0;

   const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

   vkb::InstanceBuilder builder;

   auto result = builder.set_app_name("Example vulkan_application")
                    .request_validation_layers(true)
                    .use_default_debug_messenger()
                    .require_api_version(1, 3, 0)
                    .build();
   vkb::Instance            vkbInstance    = result.value();
   VkInstance               instance       = vkbInstance.instance;
   VkDebugUtilsMessengerEXT debugMessenger = vkbInstance.debug_messenger;

   VkSurfaceKHR surface{};
   SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

   VkPhysicalDeviceVulkan13Features features{
      .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .synchronization2 = true,
      .dynamicRendering = true,
   };

   VkPhysicalDeviceVulkan12Features features12{
      .sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .descriptorIndexing = true,
      .shaderSampledImageArrayNonUniformIndexing = true,
      .descriptorBindingVariableDescriptorCount  = true,
      .runtimeDescriptorArray                    = true,
      .bufferDeviceAddress                       = true,
   };

   VkPhysicalDeviceFeatures enabledVk10Features = {
      .samplerAnisotropy = VK_TRUE,
   };

   // VkPhysicalDeviceVulkan11Features features11{
   //    .sType                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
   //    .variablePointersStorageBuffer = true,
   //    .variablePointers              = true,
   // };

   vkb::PhysicalDeviceSelector selector{vkbInstance};

   vkb::PhysicalDevice vkbPhysicalDevice = selector.set_minimum_version(1, 3)
                                              .set_required_features_13(features)
                                              .set_required_features_12(features12)
                                              .set_required_features(enabledVk10Features)
                                              // .set_required_features_11(features11)
                                              .set_surface(surface)
                                              .select()
                                              .value();

   vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
   vkb::Device        vkbDevice = deviceBuilder.build().value();

   VkDevice         device         = vkbDevice.device;
   VkPhysicalDevice physicalDevice = vkbPhysicalDevice.physical_device;

   VkQueue graphicsQueue       = vkbDevice.get_queue(vkb::QueueType::graphics).value();
   u32     graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

   VmaVulkanFunctions vkFunctions = {
      .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
      .vkGetDeviceProcAddr   = vkGetDeviceProcAddr,
      .vkCreateImage         = vkCreateImage,
   };

   VmaAllocatorCreateInfo allocatorCI = {
      .flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice   = physicalDevice,
      .device           = device,
      .pVulkanFunctions = &vkFunctions,
      .instance         = instance,
   };

   VmaAllocator allocator{};
   Chk(vmaCreateAllocator(&allocatorCI, &allocator));

   VkSurfaceCapabilitiesKHR surfaceCaps{};
   Chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

   VkExtent2D swapchainExtent{surfaceCaps.currentExtent};
   if (surfaceCaps.currentExtent.width == 0xFFFFFFFF)
   {
      swapchainExtent = {
         .width  = WIDTH,
         .height = HEIGHT,
      };
   }

   const VkFormat           imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
   VkSwapchainCreateInfoKHR swapchainCI{
      .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface         = surface,
      .minImageCount   = surfaceCaps.minImageCount,
      .imageFormat     = imageFormat,
      .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
      .imageExtent =
         {
            .width  = swapchainExtent.width,
            .height = swapchainExtent.height,
         },
      .imageArrayLayers = 1,
      .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      // effectively "v-sync" mode, available everywhere
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
   };

   VkSwapchainKHR swapchain{};

   Chk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));

   u32 imageCount = 0;
   Chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
   std::vector<VkImage>     swapchainImages(imageCount);
   std::vector<VkImageView> swapchainViews(imageCount);
   Chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));

   std::array depthFormatList{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
   VkFormat   depthFormat{};
   for (auto &format : depthFormatList)
   {
      VkFormatProperties2 formatProperties{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
      vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);
      if (
         formatProperties.formatProperties.optimalTilingFeatures
         & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
      {
         depthFormat = format;
         break;
      }
   }
   VkImageCreateInfo depthImageCI = {
      .sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format    = depthFormat,
      .extent{
         .width  = static_cast<uint32_t>(WIDTH),
         .height = static_cast<uint32_t>(HEIGHT),
         .depth  = 1},
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = VK_SAMPLE_COUNT_1_BIT,
      .tiling        = VK_IMAGE_TILING_OPTIMAL,
      .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
   };

   VmaAllocationCreateInfo allocCI = {
      .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
   };
   VkImage       depthImage{};
   VmaAllocation depthImageAllocation{};
   Chk(vmaCreateImage(
      allocator,
      &depthImageCI,
      &allocCI,
      &depthImage,
      &depthImageAllocation,
      nullptr));

   VkImageViewCreateInfo depthViewCI = {
      .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image    = depthImage,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format   = depthFormat,
      .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1},
   };

   VkImageView depthImageView{};

   Chk(vkCreateImageView(device, &depthViewCI, nullptr, &depthImageView));

   tinyobj::attrib_t                attrib;
   std::vector<tinyobj::shape_t>    shapes;
   std::vector<tinyobj::material_t> materials;
   tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, "assets/suzanne.obj");

   struct Vertex
   {
      glm::vec3 pos;
      glm::vec3 normal;
      glm::vec2 uv;
   };

   const VkDeviceSize    indexCount{shapes[0].mesh.indices.size()};
   std::vector<Vertex>   vertices{};
   std::vector<uint16_t> indices{};
   // Load vertex and index data
   for (auto &index : shapes[0].mesh.indices)
   {
      Vertex v{
         .pos =
            {attrib.vertices[index.vertex_index * 3],
             -attrib.vertices[index.vertex_index * 3 + 1],
             attrib.vertices[index.vertex_index * 3 + 2]},
         .normal =
            {attrib.normals[index.normal_index * 3],
             -attrib.normals[index.normal_index * 3 + 1],
             attrib.normals[index.normal_index * 3 + 2]},
         .uv = {
            attrib.texcoords[index.texcoord_index * 2],
            1.0 - attrib.texcoords[index.texcoord_index * 2 + 1]}};
      vertices.push_back(v);
      indices.push_back(indices.size());
   }

   VkDeviceSize       vBufSize{sizeof(Vertex) * vertices.size()};
   VkDeviceSize       iBufSize{sizeof(uint16_t) * indices.size()};
   VkBufferCreateInfo bufferCI = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size  = vBufSize + iBufSize,
      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
   };

   VmaAllocationCreateInfo vBufferAllocCI{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
               | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
               | VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO};
   VmaAllocationInfo vBufferAllocInfo{};

   VkBuffer      vBuffer{};
   VmaAllocation vBufferAllocation{};

   Chk(vmaCreateBuffer(
      allocator,
      &bufferCI,
      &vBufferAllocCI,
      &vBuffer,
      &vBufferAllocation,
      &vBufferAllocInfo));

   memcpy(vBufferAllocInfo.pMappedData, vertices.data(), vBufSize);
   memcpy(static_cast<char *>(vBufferAllocInfo.pMappedData) + vBufSize, indices.data(), iBufSize);

   std::array<ShaderDataBuffer, MAX_FRAMES_IN_FLIGHT> shaderDataBuffers;

   for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
   {
      VkBufferCreateInfo uBufferCI{
         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
         .size  = sizeof(ShaderData),
         .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
      VmaAllocationCreateInfo uBufferAllocCI{
         .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                  | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                  | VMA_ALLOCATION_CREATE_MAPPED_BIT,
         .usage = VMA_MEMORY_USAGE_AUTO};
      Chk(vmaCreateBuffer(
         allocator,
         &uBufferCI,
         &uBufferAllocCI,
         &shaderDataBuffers[i].buffer,
         &shaderDataBuffers[i].allocation,
         &shaderDataBuffers[i].allocationInfo));
      VkBufferDeviceAddressInfo uBufferBdaInfo{
         .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
         .buffer = shaderDataBuffers[i].buffer};
      shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &uBufferBdaInfo);
   }

   return {
      .device               = device,
      .chosenGPU            = physicalDevice,
      .graphicsQueue        = graphicsQueue,
      .graphicsQueueFamily  = graphicsQueueFamily,
      .allocator            = allocator,
      .surface              = surface,
      .surfaceCaps          = surfaceCaps,
      .swapchain            = swapchain,
      .swapchainImages      = std::move(swapchainImages),
      .swapchainViews       = std::move(swapchainViews),
      .depthImage           = depthImage,
      .depthImageAllocation = depthImageAllocation,
      .depthImageView       = depthImageView,
      .vBuffer              = vBuffer,
      .vBufferAllocation    = vBufferAllocation,
      .shaderDataBuffers    = shaderDataBuffers,
   };
}

} // namespace kronk

int main(int argc, char **argv)
{
   if (!SDL_Init(SDL_INIT_VIDEO))
   {
      std::println("err: {}", SDL_GetError());
      assert(0);
   }
   SDL_Window *window = SDL_CreateWindow("SDL3 GL", 800, 600, SDL_WINDOW_VULKAN);
   // if (!SDL_Vulkan_LoadLibrary(nullptr))
   // {
   //   std::println("err: {}", SDL_GetError());
   //   assert(0);
   // }

   const auto vkCtx = kronk::Init(window);

   bool      running = true;
   SDL_Event e;
   while (running)
   {
      while (SDL_PollEvent(&e))
      {
         if (e.type == SDL_EVENT_QUIT)
         {
            running = false;
         }
      }
      SDL_PumpEvents();
   }
   SDL_DestroyWindow(window);
   SDL_Quit();

   return 0;
}
