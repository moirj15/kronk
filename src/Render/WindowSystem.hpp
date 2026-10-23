#pragma once
#include "./utils.hpp"

namespace kronk::vk
{

class WindowSystem final
{
   u32          width;
   u32          height;
   VkSurfaceKHR surface;

public:
   explicit WindowSystem(const u32 width, const u32 height, const VkSurfaceKHR surface) :
         width{width},
         height{height},
         surface{surface}
   {
   }

   void UpdateDimensions(this WindowSystem &self, u32 width, u32 height);

   u32 GetWidth(this const WindowSystem &self);

   u32 GetHeight(this const WindowSystem &self);

   VkSurfaceKHR GetSurface(this const WindowSystem &self);
};

} // namespace kronk::vk
