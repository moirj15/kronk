#pragma once

#include "../utils.hpp"

#include <print>
#include <vulkan/vulkan.h>

namespace kronk::vk
{

constexpr void Chk(VkResult result)
{
   if (result != VK_SUCCESS)
   {
      std::println("Vulkan call returned an error {}", static_cast<u32>(result));
      std::exit(result);
   }
}
} // namespace kronk::vk