#include "DescriptorAllocatorPage.h"
#include "Application.h"
#include "DescriptorAllocation.h"

#include <assert.h>
#include <d3dx12.h>

DescriptorAllocatorPage::DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorType, UINT NumDescriptors)
    : m_DescriptorHeapType(DescriptorType)
    , m_CurrentFreeNumHandle(NumDescriptors)
{
    //Initialize private members
    m_DescriptorHeap = Application::GetApp()->CreateDescriptorHeap(m_DescriptorHeapType, m_CurrentFreeNumHandle);
    m_BaseDescriptorCpuHandle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_DescriptorIncrementSize = Application::GetApp()->GetDescriptorIncrementSize(m_DescriptorHeapType);
    //Push the whole descriptor heap into the free block.
    AddNewFreeBlock(0, m_CurrentFreeNumHandle);
}

DescriptorAllocatorPage::~DescriptorAllocatorPage()
{
}

DescriptorAllocation DescriptorAllocatorPage::Allocate(UINT NumDescriptors)
{
    //-------------------------------------------------
    std::lock_guard<std::mutex> lock(m_AllocationMutex);
    //-------------------------------------------------

    DescriptorAllocation allocation;
    //If current descriptor heap has no enough room to allocate,return null
    if (NumDescriptors > m_CurrentFreeNumHandle)
    {
        return DescriptorAllocation();
    }
    //Find the first free block which is greater or equal with the NumDescriptors
    auto firstPos = m_FreeBlockBySize.lower_bound(NumDescriptors);
    //If there is no free block to satisfy, then return null
    if (firstPos == m_FreeBlockBySize.end())
    {
        return DescriptorAllocation();
    }
    //If the code run here,we can start to allocate.
    UINT freeBlockOffset = firstPos->second->first;
    UINT freeBlockSize = firstPos->first;
    //Update offset and size after allocation
    UINT newOffset = freeBlockOffset + NumDescriptors;
    UINT newSize = freeBlockSize - NumDescriptors;
    assert(newSize >= 0 && "Invalid Size Value");
    //Erase old free block from containers.
    m_FreeBlockByOffset.erase(firstPos->second);
    m_FreeBlockBySize.erase(firstPos);
    //If free block is run out of,we do nothing.If not,we push remain as free block into containers.
    if (newSize != 0)
    {
        AddNewFreeBlock(newOffset, newSize);
    }
    //Do not forget to update current free handles in descirptor heap.
    m_CurrentFreeNumHandle -= NumDescriptors;
    return DescriptorAllocation(CD3DX12_CPU_DESCRIPTOR_HANDLE(m_BaseDescriptorCpuHandle, freeBlockOffset, m_DescriptorIncrementSize),
        NumDescriptors, m_DescriptorIncrementSize,shared_from_this());
}

UINT DescriptorAllocatorPage::FreeNumHandle()const
{
    return m_CurrentFreeNumHandle;
}

void DescriptorAllocatorPage::AddNewFreeBlock(UINT BlockOffset, UINT BlockSize)
{
    auto OffsetIt = m_FreeBlockByOffset.emplace(BlockOffset, BlockSize);
    auto SizeIt = m_FreeBlockBySize.emplace(BlockSize, OffsetIt.first);
    OffsetIt.first->second.FreeBlockBySizeIt = SizeIt;
}

bool DescriptorAllocatorPage::HasSapce(UINT NumDescriptors)
{
    return m_FreeBlockBySize.lower_bound(NumDescriptors) != m_FreeBlockBySize.end();
}

void DescriptorAllocatorPage::ReleaseBlock(UINT Offset, UINT NumDescriptors)
{
    //the first iterator which is greater than offset.
    //if the iterator is upper-end iterator,that means the block is the last one.
    auto nextBlockByOffsetIt = m_FreeBlockByOffset.upper_bound(Offset);
    //the previous block iterator
    //Note do not adjust nextBlockByOffsetIt...
    auto prevBlockByOffsetIt = nextBlockByOffsetIt;
    //If the block is front of all free blocks,we set prevBlockIt to end.
    if (prevBlockByOffsetIt == m_FreeBlockByOffset.begin())
    {
        prevBlockByOffsetIt = m_FreeBlockByOffset.end();
    }
    else
    {
        --prevBlockByOffsetIt;
    }

    UINT newOffset = Offset;
    UINT newSize = NumDescriptors;
    //If the block is not the first one in container.
    if (prevBlockByOffsetIt != m_FreeBlockByOffset.end())
    {
        /************************************************************************/
        /*  PrveOffset                             Offset           
                |             Size                   |      Size      |
                |<---------------------------------->|<-------------->|                
        /*      |-----------------------------------------------------|
        /************************************************************************/
        UINT prevBlockOffset = prevBlockByOffsetIt->first;
        UINT prevBlockSize = prevBlockByOffsetIt->second.Size;

        assert(prevBlockOffset + prevBlockSize <= Offset && "Invalid Offset");
        //Check if new free block can merge with the previous one.
        if (prevBlockOffset + prevBlockSize == Offset)
        {
            m_FreeBlockBySize.erase(prevBlockByOffsetIt->second.FreeBlockBySizeIt);
            m_FreeBlockByOffset.erase(prevBlockByOffsetIt);

            newOffset = prevBlockOffset;
            newSize += prevBlockSize;
        }
    }
    //If the block is not the last one in container.
    if (nextBlockByOffsetIt != m_FreeBlockByOffset.end())
    {
        /************************************************************************/
        /*    Offset                      NextOffset
                |             Size            |         Size          | 
                |<--------------------------->|<--------------------->|
        /*      |-----------------------------------------------------|
        /************************************************************************/
        UINT nextBlockOffset = nextBlockByOffsetIt->first;
        UINT nextBlockSize = nextBlockByOffsetIt->second.Size;

        assert(Offset + NumDescriptors <= nextBlockOffset && "Invalid Offset");
        //Check if new free block can merge with the next one.
        if (Offset + NumDescriptors == nextBlockOffset)
        {
            m_FreeBlockBySize.erase(nextBlockByOffsetIt->second.FreeBlockBySizeIt);
            m_FreeBlockByOffset.erase(nextBlockByOffsetIt);

            newOffset = newOffset;
            newSize += nextBlockSize;
        }
    }

    //Do not forget update current handle num in descirptor heap
    m_CurrentFreeNumHandle += NumDescriptors;
    //Finally,we add a new free block.
    AddNewFreeBlock(newOffset, newSize);
}

UINT DescriptorAllocatorPage::ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE Descriptor)
{
    return (Descriptor.ptr - m_BaseDescriptorCpuHandle.ptr) / m_DescriptorIncrementSize;
}

void DescriptorAllocatorPage::Free(DescriptorAllocation&& descirptorAllocation, UINT64 frame)
{
    UINT offset = ComputeOffset(descirptorAllocation.GetDescriptorHandle());
    UINT size = descirptorAllocation.GetNumHandles();
    //--------------------------------------------------------
    std::lock_guard<std::mutex> lock(m_AllocationMutex);
    //--------------------------------------------------------
    m_StaleDescriptorQueue.emplace(offset, size, frame);
}

void DescriptorAllocatorPage::ReleaseStaleDescriptors(UINT CurrentFrame)
{
    //-----------------------------------------------------
    std::lock_guard<std::mutex> lock(m_AllocationMutex);
    //-----------------------------------------------------

    //Note: the CurrentFrame is biggest frame that we can safely release.
    //So we must make sure that the descirptors which frame is greater than currentFrame in stale descirptor queue
    //will not be reused.
    while (!m_StaleDescriptorQueue.empty() && m_StaleDescriptorQueue.front().Frame <= CurrentFrame)
    {
        ReleaseBlock(m_StaleDescriptorQueue.front().Offset, m_StaleDescriptorQueue.front().Size);
        
        m_StaleDescriptorQueue.pop();
    }
}

