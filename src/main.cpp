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

constexpr u32 WIDTH  = 800;
constexpr u32 HEIGHT = 600;

namespace kronk
{

struct VkContext
{
   VkDevice                 device{};
   VkPhysicalDevice         chosenGPU{};
   VkQueue                  graphicsQueue{};
   u32                      graphicsQueueFamily{};
   VmaAllocator             allocator{};
   VkSurfaceKHR             surface{};
   VkSurfaceCapabilitiesKHR surfaceCaps{};
   VkSwapchainKHR           swapchain{};
   std::vector<VkImage>     swapchainImages{};
   std::vector<VkImageView> swapchainViews{};
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

   return {
      .device              = device,
      .chosenGPU           = physicalDevice,
      .graphicsQueue       = graphicsQueue,
      .graphicsQueueFamily = graphicsQueueFamily,
      .allocator           = allocator,
      .surface             = surface,
      .surfaceCaps         = surfaceCaps,
      .swapchain           = swapchain,
      .swapchainImages     = std::move(swapchainImages),
      .swapchainViews      = std::move(swapchainViews),
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
