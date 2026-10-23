#include "Render/WindowSystem.hpp"

namespace kronk::vk
{

void WindowSystem::UpdateDimensions(this WindowSystem &self, const u32 width, const u32 height)
{
   self.width  = width;
   self.height = height;
}

u32 WindowSystem::GetWidth(this const WindowSystem &self)
{
   return self.width;
}
u32 WindowSystem::GetHeight(this const WindowSystem &self)
{
   return self.height;
}
VkSurfaceKHR WindowSystem::GetSurface(this const WindowSystem &self)
{
   return self.surface;
}

} // namespace kronk::vk
