#include "DescriptorAllocation.h"
#include "DescriptorAllocatorPage.h"
#include "Application.h"
#include <d3dx12.h>
#include <assert.h>

DescriptorAllocation::DescriptorAllocation()
    :m_DescriptorCpuHandle(D3D12_CPU_DESCRIPTOR_HANDLE())
    , m_DescirptorSize(0)
    , m_DescriptorIncrementSize(0)
    , m_AllocationInPage(nullptr)
{
}

DescriptorAllocation::DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE DescriptorCpuHandle, UINT DescriptorSize, 
    UINT DescriptorIncrementSize,std::shared_ptr<DescriptorAllocatorPage> pAllocatorPage)
    :m_DescriptorCpuHandle(DescriptorCpuHandle)
    ,m_DescirptorSize(DescriptorSize)
    ,m_DescriptorIncrementSize(DescriptorIncrementSize)
    ,m_AllocationInPage(pAllocatorPage)
{
}

DescriptorAllocation::~DescriptorAllocation()
{
    //Note: when desciptor allocation is normally destoryed, we must give back these descriptors.
    Free();
}

void DescriptorAllocation::Free()
{
    if (!IsNull() && m_AllocationInPage)
    {
        m_AllocationInPage->Free(std::move(*this), Application::GetApp()->GetFrameCount());

        m_DescriptorCpuHandle.ptr = 0;
        m_DescriptorIncrementSize = 0;
        m_DescirptorSize = 0;
        m_AllocationInPage = nullptr;
    }
}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& move)noexcept
    :m_DescriptorCpuHandle(move.m_DescriptorCpuHandle)
    , m_DescirptorSize(move.m_DescirptorSize)
    , m_DescriptorIncrementSize(move.m_DescriptorIncrementSize)
    , m_AllocationInPage(std::move(move.m_AllocationInPage))
{
    move.m_DescriptorCpuHandle.ptr = 0;
    move.m_DescirptorSize = 0;
    move.m_DescriptorIncrementSize = 0;
    move.m_AllocationInPage = nullptr;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& moveassign)noexcept
{
    if (this != &moveassign)
    {
        Free();

        m_DescriptorCpuHandle = moveassign.m_DescriptorCpuHandle;
        m_DescriptorIncrementSize = moveassign.m_DescriptorIncrementSize;
        m_DescirptorSize = moveassign.m_DescirptorSize;
        //Note: for intelligent pointer,we must use std::move() to safely move instead of operator=()
        //Otherwise, this will lead memory leak.
        m_AllocationInPage = std::move(moveassign.m_AllocationInPage);

        moveassign.m_DescriptorCpuHandle.ptr = 0;
        moveassign.m_DescirptorSize = 0;
        moveassign.m_DescriptorIncrementSize = 0;
        moveassign.m_AllocationInPage = nullptr;
    }
    return *this;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetDescriptorHandle(UINT offset)const
{
    assert(offset < m_DescirptorSize);
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_DescriptorCpuHandle, offset, m_DescriptorIncrementSize);
}

UINT DescriptorAllocation::GetNumHandles()const
{
    return m_DescirptorSize;
}

bool DescriptorAllocation::IsNull()
{
    return m_DescirptorSize == 0;
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocation::GetAllocatorPageInAllocation()const
{
    return m_AllocationInPage;
}