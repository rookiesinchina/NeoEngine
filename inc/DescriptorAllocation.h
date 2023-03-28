#pragma once

#include <memory>
#include <d3d12.h>

//The DescriptorAllocation class is wrapper class which is responsible for handling specific descirptors in a descriptor heap.
//Every time when we need descriptors in CPU,we will get a DescriptorAllocation object which contains a consistent descriptors.
//And the DescriptorAllocation object is forbidden to copy and assign since this object will automatically free after use.
//The so-called "Free" actually is to tell the DescriptorAllocator,which manages all descirptor heap,these descriptors can be reused.

class DescriptorAllocatorPage;

class DescriptorAllocation
{
public:
    DescriptorAllocation();
    DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE DescriptorCpuHandle, UINT DescriptorSize,
        UINT DescriptorIncrementSize,std::shared_ptr<DescriptorAllocatorPage> pAllocatorPage);
    //we forbid copy and copy assign function,so this class is simliar with unique_ptr().
    //Since we do not want several objects contain same descirptors.
    //This will lead same descirptors will be free many times.
    DescriptorAllocation(const DescriptorAllocation& copy) = delete;
    DescriptorAllocation& operator=(const DescriptorAllocation& assign) = delete;
    //Since we must define move and move assign function for this class
    DescriptorAllocation(DescriptorAllocation&& move)noexcept;
    DescriptorAllocation& operator=(DescriptorAllocation&& assignmove)noexcept;

    ~DescriptorAllocation();
    /**
     * Get a specific descirptor in one allocation
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(UINT offset = 0)const;
    /**
     * Get handles number in one allocation
     */
    UINT GetNumHandles()const;
    /**
     * Check if this allocation is null
     */
    bool IsNull();
    /**
     * Tell Page that these descirptors can reuse.
     * This function must be used when desctruction or move assign.
     */
    void Free();
    /**
     * Get the page pointer which the allocation in
     */
    std::shared_ptr<DescriptorAllocatorPage> GetAllocatorPageInAllocation()const;

private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_DescriptorCpuHandle;
    UINT m_DescirptorSize;
    UINT m_DescriptorIncrementSize;
    std::shared_ptr<DescriptorAllocatorPage> m_AllocationInPage;
};