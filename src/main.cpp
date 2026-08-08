#include "utils.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <print>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

constexpr u32 WIDTH  = 1920;
constexpr u32 HEIGHT = 1080;

namespace kronk
{

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

  u32                sdlExtensionCount = 0;
  const char *const *extensions        = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

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
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features.dynamicRendering = true;
  features.synchronization2 = true;

  VkPhysicalDeviceVulkan12Features features12{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  features12.bufferDeviceAddress = true;
  features12.descriptorIndexing  = true;

  VkPhysicalDeviceVulkan11Features features11{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  features11.variablePointers              = true;
  features11.variablePointersStorageBuffer = true;

  vkb::PhysicalDeviceSelector selector{vkbInstance};

  vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3)
                                         .set_required_features_13(features)
                                         .set_required_features_12(features12)
                                         .set_required_features_11(features11)
                                         .set_surface(surface)
                                         .select()
                                         .value();

  vkb::DeviceBuilder deviceBuilder{physicalDevice};
  vkb::Device        vkbDevice = deviceBuilder.build().value();

  VkDevice         device    = vkbDevice.device;
  VkPhysicalDevice chosenGPU = physicalDevice.physical_device;

  VkQueue graphicsQueue       = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  u32     graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

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
