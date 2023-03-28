#pragma once

#include "d3dx12.h"
#include <wrl.h>
#include <memory>
#include <queue>
#include <functional>

class RootSignature;
class CommandList;

class DynamicDescriptorHeap
{
public:
    DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, UINT HeapSize = 1024);
    ~DynamicDescriptorHeap();

    void ParseRootSignature(const RootSignature& rootSignature);

    void StageDescriptors(const D3D12_CPU_DESCRIPTOR_HANDLE BaseDescriptorHandle, UINT ParameterIndex, UINT Offset, UINT DescriptorsNum);

    //Commit descriptor to commandlist graphics draw.
    void CommittedStagedDescriptorsForDraw(CommandList& commandList);
    //Commit descriptors to commandlist compute dispatch.
    void CommittedStagedDescriptorsForDispatch(CommandList& commandList);
    //Only copy one descriptor from CPU to GPU.
    //This is useful for the ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat() and ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint() functions.
    D3D12_GPU_DESCRIPTOR_HANDLE CopySingleDescriptor(CommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);

    void Reset();

protected:
    //We should not invoke this function directly instead of invoking 
    //CommittedStagedDescriptorsForDraw() or CommittedStagedDescriptorForDispatch() which will invoke this function interior.
    void CommittedStagedDescriptors(CommandList& commandList, std::function<void(ID3D12GraphicsCommandList2*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFun);
    //Get a descriptorheap from available descriptorheap,if available descriptorheap is empty then create a new one.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RequestDescriptorHeap();
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap();
    //Compute descriptors number in each descriptor table.
    UINT ComputeStageDescriptorNum();
private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_CurrentGpuDescriptorHeap;

    UINT m_DescriptorHeapSize;

    D3D12_DESCRIPTOR_HEAP_TYPE m_DescriptorHeapType;
    D3D12_CPU_DESCRIPTOR_HANDLE m_CurrentCpuDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_CurrentGpuDescriptorHandle;

    struct DescriptorTableCache
    {
        DescriptorTableCache()
        {
            BaseDescriptor = nullptr;
            DescriptorsNum = 0;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor;
        UINT DescriptorsNum;

        void Reset()
        {
            BaseDescriptor = nullptr;
            DescriptorsNum = 0;
        }
    };

    const static int m_MaxNumDescriptorTable = 32;
    //-------------------------------------------------------------------------
    // ----
    // |  | <----- a descriptor from some allocation  
    // ---- 
    //   \
    //    \  <------- Stage: pass a secific descriptor to Cpu cache (Second)
    //     \
    //      \        DescriptorCpuCache(Use number to represent address)
    // -----------------------------------------------------------------------
    // | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
    // -----------------------------------------------------------------------
    //   |         /                    /
    //   |        /                    / 
    //   |       /                    /
    //   |      /          -----------   ParseRootSignature will finish linking CpuCache and Table Cache(First)
    //   |     /          /
    //   |    /          /
    //   |   /          /
    // -------------------------------------------------------------------------
    // | 0 | 3 | null | 8 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |  <----BaseDescriptor
    // | 3 | 5 |   0  | 3 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |  <----DescriptorsNum
    // -------------------------------------------------------------------------
    //                   DescriptorTableCache(Max volume is 32)
    //--------------------------------------------------------------------------

    //The array is for storing all descirptor in tables,this is for pointer for Table Cache.
    std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> m_DescriptorCpuCache;
    //A cache is for all descriptor in rootsignature,when stage operation,necessary descriptors will pointer this cache 
    DescriptorTableCache m_DescriptorTableCache[m_MaxNumDescriptorTable];

    using DescriptorHeapPool = std::queue<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>>;

    DescriptorHeapPool m_AllDescriptorHeapPool;
    DescriptorHeapPool m_AvailDescriptorHeapPool;

    UINT m_CurrentFreeNumHandle;

    UINT m_DescriptorIncrementSize;

    UINT m_DescriptorTableBitMask;
    UINT m_StageDescriptorTableBitMask;
};

