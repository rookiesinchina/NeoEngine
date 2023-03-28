#include "DynamicDescriptorHeap.h"
#include "Application.h"
#include "RootSignature.h"
#include "d3dUtil.h"
#include "CommandList.h"
#include <stdexcept>

DynamicDescriptorHeap::DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, UINT HeapSize /* = 1024 */)
    :m_DescriptorHeapType(HeapType)
    ,m_DescriptorHeapSize(HeapSize)
    ,m_CurrentGpuDescriptorHeap(nullptr)
    ,m_DescriptorTableCache{}
    ,m_CurrentFreeNumHandle(0)
    ,m_DescriptorIncrementSize(0)
    ,m_DescriptorTableBitMask(0)
    ,m_StageDescriptorTableBitMask(0)
{
    m_DescriptorIncrementSize = Application::GetApp()->GetDescriptorIncrementSize(m_DescriptorHeapType);

    m_DescriptorCpuCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(m_DescriptorHeapSize);
}

DynamicDescriptorHeap::~DynamicDescriptorHeap()
{

}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature)
{
    //Reset all staged root descriptor bit mask.
    m_StageDescriptorTableBitMask = 0;
    //
    m_DescriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(m_DescriptorHeapType);

    UINT descriptorTableMask = m_DescriptorTableBitMask;

    DWORD rootIndex = 0;
    UINT offset = 0;
    while (_BitScanForward(&rootIndex, descriptorTableMask) && rootIndex < m_MaxNumDescriptorTable)
    {
        UINT descriptorNum = rootSignature.GetNumDescriptorsInTable(rootIndex);
        assert(descriptorNum > 0 && "Invalid Descriptors Number");

        m_DescriptorTableCache[rootIndex].BaseDescriptor = m_DescriptorCpuCache.get() + offset;
        m_DescriptorTableCache[rootIndex].DescriptorsNum = descriptorNum;

        offset += descriptorNum;
        assert(offset <= m_DescriptorHeapSize && "The DescriptorHeap Size Is Not Big Enough");

        //Filp the mask bit to prevent scan again.
        descriptorTableMask ^= (1 << rootIndex);
    }
}

void DynamicDescriptorHeap::StageDescriptors(const D3D12_CPU_DESCRIPTOR_HANDLE BaseDescriptorHandle, UINT ParameterIndex, UINT Offset, UINT DescriptorsNum)
{
    UINT DescriptorNumInTable = m_DescriptorTableCache[ParameterIndex].DescriptorsNum;
    assert(DescriptorNumInTable > 0 && "There Is No Descriptor In This Table or this root parameter is not descriptor table type");

    if (DescriptorsNum > m_DescriptorHeapSize || ParameterIndex > m_MaxNumDescriptorTable)
    {
        throw std::bad_alloc();
    }

    if (Offset + DescriptorsNum > DescriptorNumInTable)
    {
        throw std::length_error("Given Descriptors Range Exceeds Descriptors Numbers In Table");
    }

    D3D12_CPU_DESCRIPTOR_HANDLE* OffsetedDescriptor = m_DescriptorTableCache[ParameterIndex].BaseDescriptor + Offset;
    for (UINT i = 0; i < DescriptorsNum; ++i)
    {
        OffsetedDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(BaseDescriptorHandle, i, m_DescriptorIncrementSize);
    }

    //Update staged descriptor table bit mask
    m_StageDescriptorTableBitMask |= 1 << ParameterIndex;
}

void DynamicDescriptorHeap::CommittedStagedDescriptors(CommandList& commandList, std::function<void(ID3D12GraphicsCommandList2*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFun)
{
    //Before commit descriptors to Gpu descriptor heap ,we need to verify if the Gpu heap has enough space
    UINT stageDescriptorNum = ComputeStageDescriptorNum();
    //If some changes in descriptor heap
    if (stageDescriptorNum > 0)
    {
        if (!m_CurrentGpuDescriptorHeap || stageDescriptorNum > m_CurrentFreeNumHandle)
        {
            m_CurrentGpuDescriptorHeap = RequestDescriptorHeap();
            m_CurrentCpuDescriptorHandle = m_CurrentGpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            m_CurrentGpuDescriptorHandle = m_CurrentGpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
            m_CurrentFreeNumHandle = m_DescriptorHeapSize;

            //Here we need use commandlist to reset descriptorheap.
            commandList.SetDescriptorHeap(m_DescriptorHeapType, m_CurrentGpuDescriptorHeap.Get());

            m_StageDescriptorTableBitMask = m_DescriptorTableBitMask;
        }

        DWORD rootIndex = 0;
        while (_BitScanForward(&rootIndex, m_StageDescriptorTableBitMask))
        {
            UINT srcDescriptorNum = m_DescriptorTableCache[rootIndex].DescriptorsNum;
            D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorStart = m_DescriptorTableCache[rootIndex].BaseDescriptor;

            D3D12_CPU_DESCRIPTOR_HANDLE dstDescriptorStart[] =
            {
                m_CurrentCpuDescriptorHandle
            };
            UINT dstDescriptorRange[] =
            {
                srcDescriptorNum
            };

            auto device = Application::GetApp()->GetDevice();
            device->CopyDescriptors(1, dstDescriptorStart, dstDescriptorRange, srcDescriptorNum,
                srcDescriptorStart, nullptr, m_DescriptorHeapType);

            //After copy,we can bind handle to commandlist
            setFun(commandList.GetGraphicsCommandList2().Get(), rootIndex, m_CurrentGpuDescriptorHandle);
            //
            m_CurrentCpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CurrentCpuDescriptorHandle, srcDescriptorNum, m_DescriptorIncrementSize);
            m_CurrentGpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_CurrentGpuDescriptorHandle, srcDescriptorNum, m_DescriptorIncrementSize);
            m_CurrentFreeNumHandle -= srcDescriptorNum;
            //
            m_StageDescriptorTableBitMask ^= (1 << rootIndex);
        }
    }
}

void DynamicDescriptorHeap::CommittedStagedDescriptorsForDraw(CommandList& commandList)
{
    CommittedStagedDescriptors(commandList,&ID3D12GraphicsCommandList2::SetGraphicsRootDescriptorTable);
}

void DynamicDescriptorHeap::CommittedStagedDescriptorsForDispatch(CommandList& commandList)
{
    CommittedStagedDescriptors(commandList, &ID3D12GraphicsCommandList2::SetComputeRootDescriptorTable);
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopySingleDescriptor(CommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
    //Check if the descriptorheap has enough space.
    if (!m_CurrentGpuDescriptorHeap || m_CurrentFreeNumHandle < 1)
    {
        m_CurrentGpuDescriptorHeap = RequestDescriptorHeap();
        m_CurrentCpuDescriptorHandle = m_CurrentGpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_CurrentGpuDescriptorHandle = m_CurrentGpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_CurrentFreeNumHandle = m_DescriptorHeapSize;

        //Here we need use commandlist to reset descriptorheap.
        commandList.SetDescriptorHeap(m_DescriptorHeapType, m_CurrentGpuDescriptorHeap.Get());

        m_StageDescriptorTableBitMask = m_DescriptorTableBitMask;
    }

    auto device = Application::GetApp()->GetDevice();
    D3D12_GPU_DESCRIPTOR_HANDLE hGpu = m_CurrentGpuDescriptorHandle;
    device->CopyDescriptorsSimple(1, m_CurrentCpuDescriptorHandle, cpuDescriptor, m_DescriptorHeapType);

    m_CurrentCpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CurrentCpuDescriptorHandle, 1, m_DescriptorIncrementSize);
    m_CurrentGpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_CurrentGpuDescriptorHandle, 1, m_DescriptorIncrementSize);
    m_CurrentFreeNumHandle -= 1;

    return hGpu;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::RequestDescriptorHeap()
{
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    if (m_AvailDescriptorHeapPool.empty())
    {
        descriptorHeap = CreateDescriptorHeap();
        m_AllDescriptorHeapPool.push(descriptorHeap);
    }
    else
    {
        descriptorHeap = m_AvailDescriptorHeapPool.front();
        m_AvailDescriptorHeapPool.pop();
    }
    return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC descirptorDesc;
    descirptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descirptorDesc.NodeMask = 0;
    descirptorDesc.NumDescriptors = m_DescriptorHeapSize;
    descirptorDesc.Type = m_DescriptorHeapType;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;

    auto device = Application::GetApp()->GetDevice();
    ThrowIfFailed(device->CreateDescriptorHeap(&descirptorDesc, IID_PPV_ARGS(&heap)));

    return heap;
}

UINT DynamicDescriptorHeap::ComputeStageDescriptorNum()
{
    UINT mask = m_StageDescriptorTableBitMask;
    DWORD rootIndex = 0;
    UINT Counter = 0;

    while (_BitScanForward(&rootIndex, mask))
    {
        Counter += m_DescriptorTableCache[rootIndex].DescriptorsNum;
        //
        mask ^= (1 << rootIndex);
    }
    return Counter;
}

void DynamicDescriptorHeap::Reset()
{
    for (int i = 0; i < m_MaxNumDescriptorTable; ++i)
    {
        m_DescriptorTableCache->Reset();
    }
    m_CurrentGpuDescriptorHeap.Reset();
    m_CurrentCpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    m_CurrentGpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    m_DescriptorTableBitMask = 0;
    m_StageDescriptorTableBitMask = 0;
    m_AvailDescriptorHeapPool = m_AllDescriptorHeapPool;
    m_CurrentFreeNumHandle = 0;

    //Do not reset unique_ptr
}