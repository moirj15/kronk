#include "Render/MemorySystem.hpp"

#include "Utils.hpp"

namespace kronk::vk
{

void BufferSystem::ShutDown()
{
   for (auto &b : mBuffers)
   {
      vmaFreeMemory(mAllocator, b.allocation);
   }
   mBuffers = {};
   mIters.clear();
}
BufferHandle BufferSystem::AllocBuffer(const size_t size)
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
      mAllocator,
      &uBufferCI,
      &uBufferAllocCI,
      &buffer.buffer,
      &buffer.allocation,
      &buffer.allocationInfo));
   VkBufferDeviceAddressInfo uBufferBdaInfo{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = buffer.buffer,
   };
   buffer.deviceAddress = vkGetBufferDeviceAddress(mDevice, &uBufferBdaInfo);

   return mBuffers.insert(buffer);
}
void BufferSystem::FreeBuffer(const BufferHandle &handle)
{
   vmaDestroyBuffer(mAllocator, handle->buffer, handle->allocation);
   mBuffers.erase(handle);
}
size_t BufferSystem::TotalAllocated() const
{
   size_t total = 0;
   for (const auto &b : mBuffers)
   {
      total += b.allocationInfo.size;
   }
   return total;
}

} // namespace kronk::vk