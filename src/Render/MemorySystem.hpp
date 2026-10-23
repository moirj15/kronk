#pragma once

#include "plf_hive.h"

#include <cstdint>
#include <vk_mem_alloc.h>

namespace kronk::vk
{

struct ShaderDataBuffer
{
   VkBuffer          buffer{};
   VmaAllocation     allocation{};
   VmaAllocationInfo allocationInfo{};
   VkDeviceAddress   deviceAddress{};
};

using BufferHandle = plf::hive<ShaderDataBuffer>::const_iterator;

class MemorySystem final
{
   plf::hive<ShaderDataBuffer> buffers{};
   VmaAllocator                allocator{};
   VkDevice                    device{};

public:
   explicit MemorySystem(const VmaAllocator allocator, const VkDevice device) :
         allocator{allocator},
         device{device}
   {
   }

   void ShutDown(this MemorySystem &self);

   BufferHandle AllocBuffer(this MemorySystem &self, size_t size);

   void FreeBuffer(this MemorySystem &self, const BufferHandle &handle);

   size_t TotalAllocated(this const MemorySystem &self);
};

} // namespace kronk::vk