#include "utils.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <print>
#include <slang-com-ptr.h>
#include <slang.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#define TINYOBJLOADER_IMPLEMENTATION

#include "Render/MemorySystem.hpp"
#include "Render/Utils.hpp"
#include "Render/WindowSystem.hpp"

#include <glm/gtc/quaternion.hpp>
#include <tiny_obj_loader.h>

#define DeleteMoveAndCopy(ClassName)                                                               \
   ClassName(const ClassName &)            = delete;                                               \
   ClassName &operator=(const ClassName &) = delete;                                               \
                                                                                                   \
   ClassName(ClassName &&)            = delete;                                                    \
   ClassName &operator=(ClassName &&) = delete;

constexpr u32 WIDTH                = 800;
constexpr u32 HEIGHT               = 600;
constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
static bool   updateSwapchain;

static void ChkSwapchain(VkResult result)
{
   if (result < VK_SUCCESS)
   {
      if (result == VK_ERROR_OUT_OF_DATE_KHR)
      {
         updateSwapchain = true;
         return;
      }
      assert(0);
   }
}

namespace kronk::vk
{

struct ShaderData
{
   glm::mat4 projection;
   glm::mat4 view;
   glm::mat4 model[3];
   glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
   uint32_t  selected{1};
};

struct _VkContext
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
   std::array<VkFence, MAX_FRAMES_IN_FLIGHT>          fences;
   std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>      imageAcquiredSemaphores;
   std::vector<VkSemaphore>                           renderCompleteSemaphores;
};

struct VkContext
{
   VkInstance       instance{};
   VkSurfaceKHR     surface{};
   VkDevice         device{};
   VkPhysicalDevice physicalDevice{};
   VkQueue          graphicsQueue{};
   u32              graphicsQueueFamily{};
};

class SwapchainSystem final
{
   VkSurfaceKHR             surface{};
   VkDevice                 device{};
   VkSwapchainKHR           swapchain{};
   std::vector<VkImage>     swapchainImages;
   std::vector<VkImageView> swapchainViews;
   u32                      width{};
   u32                      height{};
   VkFormat                 format{VK_FORMAT_B8G8R8A8_SRGB};
   u32                      currentImage{};

public:
   explicit SwapchainSystem(u32 width, u32 height, u32 minImageCount)
   {
      Recreate(width, height, minImageCount);
   }

   DeleteMoveAndCopy(SwapchainSystem);
   VkImageView GetCurrentImageView(this const SwapchainSystem &self)
   {
      assert(self.currentImage < self.swapchainViews.size());
      return self.swapchainViews[self.currentImage];
   }

   VkImage GetCurrentImage(this const SwapchainSystem &self)
   {
      assert(self.currentImage < self.swapchainImages.size());
      return self.swapchainImages[self.currentImage];
   }

   void LoadNextImage(this SwapchainSystem &self, VkSemaphore frameSemaphore)
   {
      ChkSwapchain(vkAcquireNextImageKHR(
         self.device,
         self.swapchain,
         UINT64_MAX,
         frameSemaphore,
         VK_NULL_HANDLE,
         &self.currentImage));
   }

   u32 GetSwapchainCount(this const SwapchainSystem &self)
   {
      return self.swapchainImages.size();
   }
   const VkFormat *GetSwapchainFormatPtr(this const SwapchainSystem &self)
   {
      return &self.format;
   }

   void Recreate(this SwapchainSystem &self, u32 newWidth, u32 newHeight, u32 minImageCount)
   {
      self.width  = newWidth;
      self.height = newHeight;
      VkSwapchainCreateInfoKHR swapchainCI{
         .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
         .surface         = self.surface,
         .minImageCount   = minImageCount,
         .imageFormat     = self.format,
         .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
         .imageExtent =
            {
               .width  = self.width,
               .height = self.height,
            },
         .imageArrayLayers = 1,
         .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
         .preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
         .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
         // effectively "v-sync" mode, available everywhere
         .presentMode  = VK_PRESENT_MODE_FIFO_KHR,
         .oldSwapchain = self.swapchain,
      };

      VkSwapchainKHR newSwapchain{};

      Chk(vkCreateSwapchainKHR(self.device, &swapchainCI, nullptr, &newSwapchain));

      self.DestroyOldSwapchain();
      self.swapchain = newSwapchain;

      u32 imageCount = 0;
      Chk(vkGetSwapchainImagesKHR(self.device, self.swapchain, &imageCount, nullptr));
      self.swapchainImages.resize(imageCount);
      self.swapchainViews.resize(imageCount);
      Chk(vkGetSwapchainImagesKHR(
         self.device,
         self.swapchain,
         &imageCount,
         self.swapchainImages.data()));
      for (u32 i = 0; i < imageCount; i++)
      {
         VkImageViewCreateInfo viewCI = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image    = self.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = self.format,
            .subresourceRange =
               {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .levelCount = 1,
                  .layerCount = 1,
               },
         };
         Chk(vkCreateImageView(self.device, &viewCI, nullptr, &self.swapchainViews[i]));
      }
   }

   VkSwapchainKHR GetSwapchain(this const SwapchainSystem &self)
   {
      return self.swapchain;
   }

private:
   void DestroyOldSwapchain(this SwapchainSystem &self)
   {
      for (auto &view : self.swapchainViews)
      {
         vkDestroyImageView(self.device, view, nullptr);
         view = {};
      }
      vkDestroySwapchainKHR(self.device, self.swapchain, nullptr);
   }
};

struct RenderTarget
{
   VkImage     colorImage{};
   VkImageView colorView{};
   VkImage     depthImage{};
   VkImageView depthView{};
};
class FinalRenderTarget final
{
   VkDevice                 device{};
   std::vector<VkSemaphore> renderCompleteSemaphores;
   SwapchainSystem         *swapchainSystem{};

   VkImage      depthTexture{};
   VkImageView  depthView{};
   VmaAllocator allocator{};

   VmaAllocation depthImageAllocation{};

public:
   explicit FinalRenderTarget(
      VkDevice         device,
      SwapchainSystem *swapchainSystem,
      VmaAllocator     allocator,
      u32              width,
      u32              height) :
         device{device},
         swapchainSystem{swapchainSystem},
         allocator{allocator}
   {
      RecreateTarget(width, height);
   }

   DeleteMoveAndCopy(FinalRenderTarget);

   VkFormat   *GetImageFormats();
   VkImage     GetNextTargetImage(this FinalRenderTarget &self);
   VkImageView GetNextTargetImageView(this FinalRenderTarget &self);

   void NewFrame(this FinalRenderTarget &self)
   {
   }

   void RecreateTarget(this FinalRenderTarget &self, u32 newWidth, u32 newHeight)
   {
      for (auto &semaphore : self.renderCompleteSemaphores)
      {
         vkDestroySemaphore(self.device, semaphore, nullptr);
      }
      self.renderCompleteSemaphores.resize(self.swapchainSystem->GetSwapchainCount());
      const VkSemaphoreCreateInfo semaphoreCI{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
      for (auto &semaphore : self.renderCompleteSemaphores)
      {
         Chk(vkCreateSemaphore(self.device, &semaphoreCI, nullptr, &semaphore));
      }
      vmaDestroyImage(self.allocator, self.depthTexture, self.depthImageAllocation);
      vkDestroyImageView(self.device, self.depthView, nullptr);
   }

   RenderTarget GetTarget(this const FinalRenderTarget &self)
   {
      return {
         .colorImage = self.swapchainSystem->GetCurrentImage(),
         .colorView  = self.swapchainSystem->GetCurrentImageView(),
         .depthImage = self.depthTexture,
         .depthView  = self.depthView,
      };
   }

   VkSwapchainKHR GetSwapchain(this const FinalRenderTarget &self)
   {
      return self.swapchainSystem->GetSwapchain();
   }
};

VkContext InitVk(SDL_Window *window)
{
   u32 w = WIDTH, h = HEIGHT;
   u32 sdlExtensionCount = 0;

   // TODO: pull out vulkan specific init
   const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

   vkb::InstanceBuilder builder;

   auto                     result         = builder.set_app_name("Example vulkan_application")
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
   return {
      .instance            = instance,
      .surface             = surface,
      .device              = device,
      .physicalDevice      = physicalDevice,
      .graphicsQueue       = graphicsQueue,
      .graphicsQueueFamily = graphicsQueueFamily,
   };
}

VmaAllocator InitVma(const VkContext &ctx)
{
   VmaVulkanFunctions vkFunctions = {
      .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
      .vkGetDeviceProcAddr   = vkGetDeviceProcAddr,
      .vkCreateImage         = vkCreateImage,
   };

   VmaAllocatorCreateInfo allocatorCI = {
      .flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice   = ctx.physicalDevice,
      .device           = ctx.device,
      .pVulkanFunctions = &vkFunctions,
      .instance         = ctx.instance,
   };

   VmaAllocator allocator{};
   Chk(vmaCreateAllocator(&allocatorCI, &allocator));
   return allocator;
}

void Init(SDL_Window *window)
{
   const VkContext ctx = InitVk(window);

   WindowSystem windowSystem{WIDTH, HEIGHT, ctx.surface};

   VmaAllocator allocator = InitVma(ctx);

   MemorySystem bufferSystem{allocator, ctx.device};

   VkSurfaceCapabilitiesKHR surfaceCaps{};
   Chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physicalDevice, ctx.surface, &surfaceCaps));

   VkExtent2D swapchainExtent{surfaceCaps.currentExtent};
   if (surfaceCaps.currentExtent.width == 0xFFFFFFFF)
   {
      swapchainExtent = {
         .width  = windowSystem.GetWidth(),
         .height = windowSystem.GetHeight(),
      };
   }

   SwapchainSystem swapchainSystem{
      swapchainExtent.width,
      swapchainExtent.height,
      surfaceCaps.minImageCount,
   };
   FinalRenderTarget finalRenderTarget{
      ctx.device,
      &swapchainSystem,
      allocator,
      swapchainExtent.width,
      swapchainExtent.height,
   };

   /*
   const VkFormat           imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
   VkSwapchainCreateInfoKHR swapchainCI{
      .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface         = ctx.surface,
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

   Chk(vkCreateSwapchainKHR(ctx.device, &swapchainCI, nullptr, &swapchain));

   u32 imageCount = 0;
   Chk(vkGetSwapchainImagesKHR(ctx.device, swapchain, &imageCount, nullptr));
   std::vector<VkImage>     swapchainImages(imageCount);
   std::vector<VkImageView> swapchainViews(imageCount);
   Chk(vkGetSwapchainImagesKHR(ctx.device, swapchain, &imageCount, swapchainImages.data()));
   for (u32 i = 0; i < imageCount; i++)
   {
      VkImageViewCreateInfo viewCI{
         .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
         .image    = swapchainImages[i],
         .viewType = VK_IMAGE_VIEW_TYPE_2D,
         .format   = imageFormat,
         .subresourceRange =
            {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
      Chk(vkCreateImageView(ctx.device, &viewCI, nullptr, &swapchainViews[i]));
   }
   */

   std::array depthFormatList{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
   VkFormat   depthFormat{};
   for (auto &format : depthFormatList)
   {
      VkFormatProperties2 formatProperties{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
      vkGetPhysicalDeviceFormatProperties2(ctx.physicalDevice, format, &formatProperties);
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
         .width  = windowSystem.GetWidth(),
         .height = windowSystem.GetHeight(),
         .depth  = 1,
      },
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

   Chk(vkCreateImageView(ctx.device, &depthViewCI, nullptr, &depthImageView));

   tinyobj::attrib_t                attrib;
   std::vector<tinyobj::shape_t>    shapes;
   std::vector<tinyobj::material_t> materials;
   std::string                      warning{};
   std::string                      error{};
   if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, "assets/suzanne.obj"))
   {
      assert(0);
   }

   struct Vertex
   {
      glm::vec3 pos;
      glm::vec3 normal;
      glm::vec2 uv;
   };

   // TODO: vertex pulling
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

   std::array<BufferHandle, MAX_FRAMES_IN_FLIGHT> shaderDataBuffers;

   for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
   {
      shaderDataBuffers[i] = bufferSystem.AllocBuffer(sizeof(ShaderData));
   }

   // TODO: Fence and semaphore utils
   VkSemaphoreCreateInfo semaphoreCI{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
   VkFenceCreateInfo     fenceCI{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT};
   std::array<VkFence, MAX_FRAMES_IN_FLIGHT>     fences{};
   std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAcquiredSemaphores{};
   for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
   {
      Chk(vkCreateFence(ctx.device, &fenceCI, nullptr, &fences[i]));
      Chk(vkCreateSemaphore(ctx.device, &semaphoreCI, nullptr, &imageAcquiredSemaphores[i]));
   }
   std::vector<VkSemaphore> renderCompleteSemaphores;
   renderCompleteSemaphores.resize(swapchainSystem.GetSwapchainCount());
   for (auto &semaphore : renderCompleteSemaphores)
   {
      Chk(vkCreateSemaphore(ctx.device, &semaphoreCI, nullptr, &semaphore));
   }

   VkCommandPoolCreateInfo commandPoolCI{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = ctx.graphicsQueueFamily,
   };
   VkCommandPool commandPool{};
   Chk(vkCreateCommandPool(ctx.device, &commandPoolCI, nullptr, &commandPool));

   VkCommandBufferAllocateInfo cbAllocCI{
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = commandPool,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
   std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};
   Chk(vkAllocateCommandBuffers(ctx.device, &cbAllocCI, commandBuffers.data()));

   // TODO: shader management
   Slang::ComPtr<slang::IGlobalSession> slangGlobalSession{};
   slang::createGlobalSession(slangGlobalSession.writeRef());

   auto slangTargets{
      std::to_array<slang::TargetDesc>({
         {
            .format  = SLANG_SPIRV,
            .profile = slangGlobalSession->findProfile("spirv_1_4"),
         },
      }),
   };

   auto slangOptions{
      std::to_array<slang::CompilerOptionEntry>({
         {
            .name = slang::CompilerOptionName::EmitSpirvDirectly,
            .value =
               {
                  .kind      = slang::CompilerOptionValueKind::Int,
                  .intValue0 = 1,
               },
         },
      }),
   };

   slang::SessionDesc slangSessionDesc{
      .targets{slangTargets.data()},
      .targetCount{SlangInt(slangTargets.size())},
      .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
      .compilerOptionEntries{slangOptions.data()},
      .compilerOptionEntryCount{uint32_t(slangOptions.size())},
   };
   Slang::ComPtr<slang::ISession> slangSession;
   slangGlobalSession->createSession(slangSessionDesc, slangSession.writeRef());

   Slang::ComPtr<slang::IModule> slangModule{
      slangSession->loadModuleFromSource("triangle", "assets/shader.slang", nullptr, nullptr),
   };
   Slang::ComPtr<ISlangBlob> spirv;
   slangModule->getTargetCode(0, spirv.writeRef());

   VkShaderModuleCreateInfo shaderModuleCI{
      .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = spirv->getBufferSize(),
      .pCode    = (uint32_t *)spirv->getBufferPointer(),
   };
   VkShaderModule shaderModule{};
   Chk(vkCreateShaderModule(ctx.device, &shaderModuleCI, nullptr, &shaderModule));

   VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .size       = sizeof(VkDeviceAddress),
   };
   VkPipelineLayoutCreateInfo pipelineLayoutCI{
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount         = 0,
      .pSetLayouts            = nullptr,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges    = &pushConstantRange,
   };
   VkPipelineLayout pipelineLayout{};
   Chk(vkCreatePipelineLayout(ctx.device, &pipelineLayoutCI, nullptr, &pipelineLayout));

   VkVertexInputBindingDescription vertexBinding{
      .binding   = 0,
      .stride    = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
   };

   std::vector<VkVertexInputAttributeDescription> vertexAttributes{
      {
         .location = 0,
         .binding  = 0,
         .format   = VK_FORMAT_R32G32B32_SFLOAT,
      },
      {
         .location = 1,
         .binding  = 0,
         .format   = VK_FORMAT_R32G32B32_SFLOAT,
         .offset   = offsetof(Vertex, normal),
      },
      {
         .location = 2,
         .binding  = 0,
         .format   = VK_FORMAT_R32G32_SFLOAT,
         .offset   = offsetof(Vertex, uv),
      },
   };

   VkPipelineVertexInputStateCreateInfo vertexInputState{
      .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount   = 1,
      .pVertexBindingDescriptions      = &vertexBinding,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
      .pVertexAttributeDescriptions    = vertexAttributes.data(),
   };

   VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
      .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
   };

   std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
      {
         .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage  = VK_SHADER_STAGE_VERTEX_BIT,
         .module = shaderModule,
         .pName  = "main",
      },
      {
         .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = shaderModule,
         .pName  = "main",
      },
   };

   VkPipelineViewportStateCreateInfo viewportState{
      .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount  = 1,
   };
   std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
   VkPipelineDynamicStateCreateInfo dynamicState{
      .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates    = dynamicStates.data(),
   };

   VkPipelineDepthStencilStateCreateInfo depthStencilState{
      .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable  = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL,
   };

   VkPipelineRenderingCreateInfo renderingCI{
      .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount    = 1,
      .pColorAttachmentFormats = swapchainSystem.GetSwapchainFormatPtr(),
      .depthAttachmentFormat   = depthFormat,
   };

   VkPipelineColorBlendAttachmentState blendAttachment{.colorWriteMask = 0xF};
   VkPipelineColorBlendStateCreateInfo colorBlendState{
      .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments    = &blendAttachment,
   };
   VkPipelineRasterizationStateCreateInfo rasterizationState{
      .sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .lineWidth = 1.0f,
   };
   VkPipelineMultisampleStateCreateInfo multisampleState{
      .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
   };

   VkGraphicsPipelineCreateInfo pipelineCI{
      .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext               = &renderingCI,
      .stageCount          = 2,
      .pStages             = shaderStages.data(),
      .pVertexInputState   = &vertexInputState,
      .pInputAssemblyState = &inputAssemblyState,
      .pViewportState      = &viewportState,
      .pRasterizationState = &rasterizationState,
      .pMultisampleState   = &multisampleState,
      .pDepthStencilState  = &depthStencilState,
      .pColorBlendState    = &colorBlendState,
      .pDynamicState       = &dynamicState,
      .layout              = pipelineLayout,
   };
   VkPipeline pipeline{};
   // TODO: PSO management
   Chk(vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

   u64       lastTime   = SDL_GetTicks();
   bool      quit       = false;
   u64       frameIndex = 0;
   u32       imageIndex = 0;
   glm::vec3 camPos{0.0f, 0.0f, -6.0f};
   glm::vec3 objectRotations[3]{};

   while (!quit)
   {
      Chk(vkWaitForFences(ctx.device, 1, &fences[frameIndex], true, UINT64_MAX));
      Chk(vkResetFences(ctx.device, 1, &fences[frameIndex]));

      for (SDL_Event event; SDL_PollEvent(&event);)
      {
         // Exit loop if the application is about to close
         if (event.type == SDL_EVENT_QUIT)
         {
            quit = true;
            break;
         }
         if (event.type == SDL_EVENT_WINDOW_RESIZED)
         {
            updateSwapchain = true;
            Chk(vkDeviceWaitIdle(ctx.device));
            Chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
               ctx.physicalDevice,
               ctx.surface,
               &surfaceCaps));
            i32 w{}, h{};
            SDL_GetWindowSize(window, (i32 *)&w, (i32 *)&h);
#if 0
            swapchainCI.oldSwapchain = swapchain;
            windowSystem.UpdateDimensions(w, h);
            swapchainCI.imageExtent = {
               .width  = windowSystem.GetWidth(),
               .height = windowSystem.GetHeight()};
            Chk(vkCreateSwapchainKHR(ctx.device, &swapchainCI, nullptr, &swapchain));
            for (auto i = 0; i < imageCount; i++)
            {
               vkDestroyImageView(ctx.device, swapchainViews[i], nullptr);
            }
            Chk(vkGetSwapchainImagesKHR(ctx.device, swapchain, &imageCount, nullptr));
            swapchainImages.resize(imageCount);
            Chk(
               vkGetSwapchainImagesKHR(ctx.device, swapchain, &imageCount, swapchainImages.data()));
            swapchainViews.resize(imageCount);
            for (auto i = 0; i < imageCount; i++)
            {
               VkImageViewCreateInfo viewCI{
                  .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                  .image    = swapchainImages[i],
                  .viewType = VK_IMAGE_VIEW_TYPE_2D,
                  .format   = imageFormat,
                  .subresourceRange =
                     {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
               Chk(vkCreateImageView(ctx.device, &viewCI, nullptr, &swapchainViews[i]));
            }
#endif
            for (auto &semaphore : renderCompleteSemaphores)
            {
               vkDestroySemaphore(ctx.device, semaphore, nullptr);
            }
            renderCompleteSemaphores.resize(swapchainSystem.GetSwapchainCount());
            for (auto &semaphore : renderCompleteSemaphores)
            {
               Chk(vkCreateSemaphore(ctx.device, &semaphoreCI, nullptr, &semaphore));
            }
            vmaDestroyImage(allocator, depthImage, depthImageAllocation);
            vkDestroyImageView(ctx.device, depthImageView, nullptr);
            depthImageCI.extent = {
               .width  = static_cast<uint32_t>(w),
               .height = static_cast<uint32_t>(h),
               .depth  = 1};
            VmaAllocationCreateInfo allocCI_{
               .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
               .usage = VMA_MEMORY_USAGE_AUTO};
            Chk(vmaCreateImage(
               allocator,
               &depthImageCI,
               &allocCI_,
               &depthImage,
               &depthImageAllocation,
               nullptr));
            VkImageViewCreateInfo viewCI{
               .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
               .image    = depthImage,
               .viewType = VK_IMAGE_VIEW_TYPE_2D,
               .format   = depthFormat,
               .subresourceRange =
                  {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}};
            Chk(vkCreateImageView(ctx.device, &viewCI, nullptr, &depthImageView));
         }
      }

      const RenderTarget renderTarget = finalRenderTarget.GetTarget();
      ShaderData         shaderData;

      shaderData.projection = glm::perspective(
         glm::radians(45.0f),
         (float)windowSystem.GetWidth() / (float)windowSystem.GetHeight(),
         0.1f,
         32.0f);

      shaderData.view = glm::translate(glm::mat4(1.0f), camPos);
      for (auto i = 0; i < 3; i++)
      {
         auto instancePos    = glm::vec3((float)(i - 1) * 3.0f, 0.0f, 0.0f);
         shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos)
                               * glm::mat4_cast(glm::quat(objectRotations[i]));
      }
      memcpy(
         shaderDataBuffers[frameIndex]->allocationInfo.pMappedData,
         &shaderData,
         sizeof(ShaderData));

      auto cb = commandBuffers[frameIndex];
      Chk(vkResetCommandBuffer(cb, 0));

      VkCommandBufferBeginInfo cbBI{
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      };
      Chk(vkBeginCommandBuffer(cb, &cbBI));

      std::array outputBarriers{
         VkImageMemoryBarrier2{
            .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask =
               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .image     = swapchainSystem.GetCurrentImage(),
            .subresourceRange{
               .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
               .levelCount = 1,
               .layerCount = 1}},
         VkImageMemoryBarrier2{
            .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .image         = depthImage,
            .subresourceRange{
               .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
               .levelCount = 1,
               .layerCount = 1}}};
      VkDependencyInfo barrierDependencyInfo{
         .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
         .imageMemoryBarrierCount = 2,
         .pImageMemoryBarriers    = outputBarriers.data()};
      vkCmdPipelineBarrier2(cb, &barrierDependencyInfo);

      VkRenderingAttachmentInfo colorAttachmentInfo{
         .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
         .imageView   = swapchainSystem.GetCurrentImageView(),
         .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
         .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
         .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
         .clearValue{.color{0.0f, 0.0f, 0.2f, 1.0f}}};
      VkRenderingAttachmentInfo depthAttachmentInfo{
         .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
         .imageView   = depthImageView,
         .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
         .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
         .storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE,
         .clearValue  = {.depthStencil = {1.0f, 0}}};

      VkRenderingInfo renderingInfo{
         .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
         .renderArea{
            .extent{
               .width  = windowSystem.GetWidth(),
               .height = windowSystem.GetHeight(),
            },
         },
         .layerCount           = 1,
         .colorAttachmentCount = 1,
         .pColorAttachments    = &colorAttachmentInfo,
         .pDepthAttachment     = &depthAttachmentInfo};
      vkCmdBeginRendering(cb, &renderingInfo);

      VkViewport vp{
         .width    = static_cast<float>(windowSystem.GetWidth()),
         .height   = static_cast<float>(windowSystem.GetHeight()),
         .minDepth = 0.0f,
         .maxDepth = 1.0f};
      vkCmdSetViewport(cb, 0, 1, &vp);
      VkRect2D scissor{
         .extent{
            .width  = windowSystem.GetWidth(),
            .height = windowSystem.GetHeight(),
         },
      };
      vkCmdSetScissor(cb, 0, 1, &scissor);

      vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      VkDeviceSize vOffset{0};
      // vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
      // &descriptorSetTex, 0, nullptr);
      vkCmdBindVertexBuffers(cb, 0, 1, &vBuffer, &vOffset);
      vkCmdBindIndexBuffer(cb, vBuffer, vBufSize, VK_INDEX_TYPE_UINT16);

      vkCmdPushConstants(
         cb,
         pipelineLayout,
         VK_SHADER_STAGE_VERTEX_BIT,
         0,
         sizeof(VkDeviceAddress),
         &shaderDataBuffers[frameIndex]->deviceAddress);

      vkCmdDrawIndexed(cb, indexCount, 3, 0, 0, 0);
      vkCmdEndRendering(cb);

      VkImageMemoryBarrier2 barrierPresent{
         .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
         .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
         .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         .dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
         .dstAccessMask = 0,
         .oldLayout     = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
         .newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
         .image         = swapchainSystem.GetCurrentImage(),
         .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1}};
      VkDependencyInfo barrierPresentDependencyInfo{
         .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
         .imageMemoryBarrierCount = 1,
         .pImageMemoryBarriers    = &barrierPresent};
      vkCmdPipelineBarrier2(cb, &barrierPresentDependencyInfo);

      vkEndCommandBuffer(cb);

      VkSemaphoreSubmitInfo waitSemaphoreInfo{
         .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
         .semaphore = imageAcquiredSemaphores[frameIndex],
         .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      };
      VkCommandBufferSubmitInfo commandBufferSubmitInfo{
         .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
         .commandBuffer = cb};
      VkSemaphoreSubmitInfo signalSemaphoreInfo{
         .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
         .semaphore = renderCompleteSemaphores[imageIndex],
         .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      };
      VkSubmitInfo2 submitInfo{
         .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
         .waitSemaphoreInfoCount   = 1,
         .pWaitSemaphoreInfos      = &waitSemaphoreInfo,
         .commandBufferInfoCount   = 1,
         .pCommandBufferInfos      = &commandBufferSubmitInfo,
         .signalSemaphoreInfoCount = 1,
         .pSignalSemaphoreInfos    = &signalSemaphoreInfo,
      };
      Chk(vkQueueSubmit2(ctx.graphicsQueue, 1, &submitInfo, fences[frameIndex]));

      frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

      VkSwapchainKHR swapchain = finalRenderTarget.GetSwapchain();

      VkPresentInfoKHR presentInfo{
         .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
         .waitSemaphoreCount = 1,
         .pWaitSemaphores    = &renderCompleteSemaphores[imageIndex],
         .swapchainCount     = 1,
         .pSwapchains        = &swapchain,
         .pImageIndices      = &imageIndex};
      ChkSwapchain(vkQueuePresentKHR(ctx.graphicsQueue, &presentInfo));
   }
}

} // namespace kronk::vk

int main(int argc, char **argv)
{
   if (!SDL_Init(SDL_INIT_VIDEO))
   {
      std::println("err: {}", SDL_GetError());
      assert(0);
   }
   SDL_Window *window =
      SDL_CreateWindow("SDL3 GL", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
   // if (!SDL_Vulkan_LoadLibrary(nullptr))
   // {
   //   std::println("err: {}", SDL_GetError());
   //   assert(0);
   // }

   kronk::vk::Init(window);

   bool      running = true;
   SDL_Event e;
   // while (running)
   // {
   //    while (SDL_PollEvent(&e))
   //    {
   //       if (e.type == SDL_EVENT_QUIT)
   //       {
   //          running = false;
   //       }
   //    }
   //    SDL_PumpEvents();
   // }
   SDL_DestroyWindow(window);
   SDL_Quit();

   return 0;
}