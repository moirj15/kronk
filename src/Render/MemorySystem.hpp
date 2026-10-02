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

class BufferSystem final
{
   plf::hive<ShaderDataBuffer> mBuffers{};
   std::vector<BufferHandle>   mIters;
   VmaAllocator                mAllocator{};
   VkDevice                    mDevice{};

public:
   explicit BufferSystem(const VmaAllocator allocator, const VkDevice device) :
         mAllocator{allocator},
         mDevice{device}
   {
   }

   void ShutDown();

   BufferHandle AllocBuffer(size_t size);

   void FreeBuffer(const BufferHandle &handle);

   size_t TotalAllocated() const;
};

} // namespace kronk::vk