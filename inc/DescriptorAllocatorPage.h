#pragma once


#include "DescriptorAllocation.h"
#include <d3d12.h>
#include <map>
#include <wrl.h>
#include <queue>
#include <mutex>



// DescriptorAlloctorPage Class actually contain the D3D12_DESCRIPTOR_HEAP instance.
// This class is responsible for managing all handles in one descriptor heap.
// Main functions: allocate descriptors, memory management and so on.


class DescriptorAllocatorPage : public std::enable_shared_from_this<DescriptorAllocatorPage>
{
public:
    DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorType, UINT NumDescriptors);
    virtual ~DescriptorAllocatorPage();

    /**
     * Check if the descriptor heap has enough room to store descriptors
     */
    bool HasSapce(UINT NumDescriptors);
    /**
     * Free allocation which is not used.
     * We do not free them directly.But push then into a queue which contains all stale descriptors
     * After a frame is completed, we use RsleaseStaleDescriptors() to safely reuse them.
     */
    void Free(DescriptorAllocation&& descirptorAllocation, UINT64 frame);
    /**
     * Allocate consistent descriptors from one page.
     * @para Descriptors number which you need
     * @return a wrapper which has handle,if allocation failure ,the descriptorallocation is NULL.
     */
    DescriptorAllocation Allocate(UINT NumDescriptors); 
    /**
     * Releasing all stale descriptors in one page.
     * Note: releasing descriptors can not be executed 
     * until the command queue of current frame does not use these descriptors anymore.
     */
    void ReleaseStaleDescriptors(UINT CurrentFrame);

    /**
     * Free handles numbers in this page
     */
    UINT FreeNumHandle()const;
protected:
    /**
     * Release descriptors which are not used.At same time.
     * if free block is neighbour,merge them to a bigger free block.
     */
    void ReleaseBlock(UINT Offset,UINT NumDescriptors);
    /**
     * Add a new free block into containers.
     */
    void AddNewFreeBlock(UINT Offset,UINT Size);
    /**
     * Compute a specific descriptor offset in descriptor heap
     */
    UINT ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE Descriptor);
private:
    struct FreeHandleInfo;
    using BlockOffset = UINT;
    using BlockSize = UINT;
    //When we allocate desriptors, descriptors fragment in descriptor heap is a very normal phenomenon.
    //So we use FreeBlockByOffset and FreeBlockBySize to achieve fastly and efficiently allocate descriptors.
    //-----------------------------------------------------------------------------------------------------
    //|<--block1->|<--fragment--->|<----block2------->|<--fragment--->|<-------block3-------->|
    //-----------------------------------------------------------------------------------------
    //| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13| 14| 15| 16| 17| 18| 19| 20| 21| <----- Descriptor Heap
    //-----------------------------------------------------------------------------------------
    //
    //
    //-----------------           
    //| offset | size |           
    //|---------------|   <------FreeBlockByOffset
    //| iter to Size  |    
    //|---------------|   
    //
    //
    //-----------------           
    //|     size      |           
    //|---------------|   <------FreeBlockBySize
    //| iter to Offset|    
    //|---------------|
    //
    using FreeBlockByOffset = std::map<BlockOffset, FreeHandleInfo>;
    using FreeBlockBySize = std::multimap<BlockSize, FreeBlockByOffset::iterator>;

    struct FreeHandleInfo
    {
        UINT Size;
        FreeBlockBySize::iterator FreeBlockBySizeIt;

        FreeHandleInfo(UINT size) : Size(size) {};
    };

    struct StaleHandleInfo
    {
        StaleHandleInfo(BlockOffset _offset, BlockSize _size, UINT _frame)
            : Offset(_offset)
            , Size(_size)
            , Frame(_frame) {};

        //the offset of first handle of stale block in descriptor heap
        BlockOffset Offset;
        //the size of stale block in descriptor heap
        BlockSize   Size;
        //current frame counter;
        UINT        Frame;
    };

    std::queue<StaleHandleInfo> m_StaleDescriptorQueue;

    UINT m_CurrentFreeNumHandle;

    FreeBlockByOffset m_FreeBlockByOffset;
    FreeBlockBySize   m_FreeBlockBySize;

    D3D12_CPU_DESCRIPTOR_HANDLE m_BaseDescriptorCpuHandle;
    UINT m_DescriptorIncrementSize;
    D3D12_DESCRIPTOR_HEAP_TYPE m_DescriptorHeapType;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;

    //-------------------------------------------------
    std::mutex m_AllocationMutex;
    //-------------------------------------------------
};