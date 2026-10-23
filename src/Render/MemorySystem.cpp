#include "Render/MemorySystem.hpp"

#include "Utils.hpp"

namespace kronk::vk
{

void MemorySystem::ShutDown(this MemorySystem &self)
{
   for (auto &b : self.buffers)
   {
      vmaFreeMemory(self.allocator, b.allocation);
   }
   self.buffers = {};
}
BufferHandle MemorySystem::AllocBuffer(this MemorySystem &self, const size_t size)
{
   ShaderDataBuffer buffer{};

   VkBufferCreateInfo uBufferCI{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size  = size,
      .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
   };
   VmaAllocationCreateInfo uBufferAllocCI{
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
               | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
               | VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO,
   };
   Chk(vmaCreateBuffer(
      self.allocator,
      &uBufferCI,
      &uBufferAllocCI,
      &buffer.buffer,
      &buffer.allocation,
      &buffer.allocationInfo));
   VkBufferDeviceAddressInfo uBufferBdaInfo{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = buffer.buffer,
   };
   buffer.deviceAddress = vkGetBufferDeviceAddress(self.device, &uBufferBdaInfo);

   return self.buffers.insert(buffer);
}
void MemorySystem::FreeBuffer(this MemorySystem &self, const BufferHandle &handle)
{
   vmaDestroyBuffer(self.allocator, handle->buffer, handle->allocation);
   self.buffers.erase(handle);
}

size_t MemorySystem::TotalAllocated(this const MemorySystem &self)
{
   size_t total = 0;
   for (const auto &b : self.buffers)
   {
      total += b.allocationInfo.size;
   }
   return total;
}

} // namespace kronk::vk