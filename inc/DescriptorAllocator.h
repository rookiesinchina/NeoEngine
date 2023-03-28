#pragma once

// This DescriptorAlloctor class is responsible for managing all DescriptorHeap page ,allocate ,free a block of
// CPU descriptors wrapped in DescriptorAllocation
#include "DescriptorAllocation.h"
#include <wrl.h>
#include <d3d12.h> 
#include <memory>
#include <vector>
#include <set>
#include <mutex>



class DescriptorAllocatorPage;


class DescriptorAllocator
{
public:
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT NumDescriptors = 256);
    ~DescriptorAllocator();

    /** 
    * Get a block of consistent descriptors in descriptor page.
    * @para: the descriptors which you need.
    * @return: descriptoralloction which wraps enough descriptors.
    */
    DescriptorAllocation Allocate(UINT NumDescriptors);

    /**
     * Releasing all descriptors which are not used anymore.
     * We can not do this until all descriptors which is referenced in current command queue are executed.
     */
    void ReleaseStaleDescriptorAlloction(UINT64 CurrentFrame);

protected:
    std::shared_ptr<DescriptorAllocatorPage> GetNewDescriptorAlloctorPage(UINT NumDescriptors);
private:
    using DescriptorHeapPool = std::vector<std::shared_ptr<DescriptorAllocatorPage>>;

    D3D12_DESCRIPTOR_HEAP_TYPE m_DescriptorHeapType;
    UINT m_NumDescriptorsPerHeap;

    DescriptorHeapPool m_HeapPool;

    std::set<DescriptorHeapPool::size_type> m_AvailableHeapIndex;

    //---------------------------------
    std::mutex m_AllocationMutex;
    //---------------------------------
};
