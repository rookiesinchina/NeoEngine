#include "DescriptorAllocator.h"
#include "DescriptorAllocatorPage.h"
#include <assert.h>
#include <mutex>

DescriptorAllocator::DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT NumDescriptors /* = 256 */)
    :m_DescriptorHeapType(heapType)
    , m_NumDescriptorsPerHeap(NumDescriptors) {};

DescriptorAllocator::~DescriptorAllocator() {};

DescriptorAllocation DescriptorAllocator::Allocate(UINT NumDescriptors)
{
    //---------------------------------------------------
    std::lock_guard<std::mutex> lock(m_AllocationMutex);
    //---------------------------------------------------

    DescriptorAllocation allocation;
    // for loop to allocate in available heap enough descriptors 
    for (const auto& heapindex : m_AvailableHeapIndex)
    {
        allocation = m_HeapPool[heapindex]->Allocate(NumDescriptors);
        //If there is no more handle in this page after allocation, we erase this page index from avail heap
        if (m_HeapPool[heapindex]->FreeNumHandle() == 0)
        {
            m_AvailableHeapIndex.erase(heapindex);
        }
        //If allocation is not null, then allocation completes.
        if (!allocation.IsNull())
        {
            break;
        }
    }
    // If there is no enough space in all available page,then we create a new page which has enough room.
    if (allocation.IsNull())
    {
        m_NumDescriptorsPerHeap = max(m_NumDescriptorsPerHeap, NumDescriptors);

        auto newPage = GetNewDescriptorAlloctorPage(m_NumDescriptorsPerHeap);
        allocation = newPage->Allocate(NumDescriptors);
        assert(!allocation.IsNull() && "Descriptor Allocation Exception");
    }

    return allocation;
}

void DescriptorAllocator::ReleaseStaleDescriptorAlloction(UINT64 CurrentFrame)
{
    //------------------------------------------------------------------
    std::lock_guard<std::mutex> lock(m_AllocationMutex);
    //------------------------------------------------------------------

    //we iterate all page in heap pool and release all stale descriptors
    for (size_t index = 0 ; index < m_HeapPool.size() ; ++index)
    {
        m_HeapPool[index]->ReleaseStaleDescriptors(CurrentFrame);

        //If there are some free handles after releasing , we push this page into available page for reuse
        if (m_HeapPool[index]->FreeNumHandle() > 0)
        {
            m_AvailableHeapIndex.insert(index);
        }
    }
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocator::GetNewDescriptorAlloctorPage(UINT NumDescriptors)
{
    std::shared_ptr<DescriptorAllocatorPage> newPage = std::make_shared<DescriptorAllocatorPage>(m_DescriptorHeapType,NumDescriptors);

    //we push this new page into heappool and available page
    m_HeapPool.push_back(newPage);
    m_AvailableHeapIndex.insert(m_HeapPool.size() - 1);

    return newPage;
}