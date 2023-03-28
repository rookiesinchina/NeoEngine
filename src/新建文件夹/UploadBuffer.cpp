#include "UploadBuffer.h"

#include "Application.h"


UploadBuffer::UploadBuffer(UINT pageSize)
    : m_PageSize(pageSize)
{};

UploadBuffer::~UploadBuffer() {};

UINT UploadBuffer::PageSize()const
{
    return m_PageSize;
}

UploadBuffer::Allocation UploadBuffer::Allocate(UINT sizeInBytes, UINT alignment)
{
    //If the data size is larger than page size,then we throw a exception.
    if (sizeInBytes > m_PageSize)
    {
        throw std::bad_alloc();
    }
    //Check if current page has enough space to store data.
    if (!m_CurrentPage || !m_CurrentPage->HasSpace(sizeInBytes, alignment))
    {
        m_CurrentPage = RequestPage();
    }

    return m_CurrentPage->Allocate(sizeInBytes, alignment);
}

void UploadBuffer::Reset()
{
    m_CurrentPage = nullptr;

    m_AvailablePagePool = m_AllPagePool;

    for (auto& Page : m_AvailablePagePool)
    {
        Page->Reset();
    }
}

UploadBuffer::Page::Page(UINT pageSize)
    :m_PageSize(pageSize)
    , m_CPUPtr(nullptr)
    , m_GPUPtr(D3D12_GPU_VIRTUAL_ADDRESS(0))
    , m_Offset(0)
{
    auto device = Application::GetApp()->GetDevice();

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(pageSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_PageResource)));

    m_GPUPtr = m_PageResource->GetGPUVirtualAddress();
    
    ThrowIfFailed(m_PageResource->Map(0, nullptr, &m_CPUPtr));
}

UploadBuffer::Page::~Page()
{
    m_PageResource->Unmap(0, 0);
    m_CPUPtr = nullptr;
    m_GPUPtr = D3D12_GPU_VIRTUAL_ADDRESS(0);
}

bool UploadBuffer::Page::HasSpace(UINT sizeInBytes, UINT alignment)
{
    UINT alignedSize = Math::AlignUp(sizeInBytes, alignment);
    UINT alignedOffset = Math::AlignUp(m_Offset, alignment);

    return alignedSize + alignedOffset <= m_PageSize;
}

UploadBuffer::Allocation UploadBuffer::Page::Allocate(UINT sizeInBytes, UINT alignment)
{
    //Since UploadBuffer::Allocate() has checked this page has enough memory to store data.
    //So there is no need to check again.
    UINT alignedOffset = Math::AlignUp(m_Offset, alignment);
    UINT alignedSize = Math::AlignUp(sizeInBytes, alignment);

    Allocation allocation;
    allocation.CPU = static_cast<UINT*>(m_CPUPtr) + alignedOffset;
    allocation.GPU = m_GPUPtr + alignedOffset;

    m_Offset = alignedOffset + alignedSize;

    return allocation;
}

void UploadBuffer::Page::Reset()
{
    m_Offset = 0;
}

std::shared_ptr<UploadBuffer::Page> UploadBuffer::RequestPage()
{
    std::shared_ptr<UploadBuffer::Page> pPage;
    //If there are available page,we use it.
    if (!m_AvailablePagePool.empty())
    {
        pPage = m_AvailablePagePool.front();
        m_AvailablePagePool.pop_front();
    }
    //If not, we create a new page.
    else
    {
        pPage = std::make_shared<UploadBuffer::Page>(m_PageSize);
        m_AllPagePool.push_back(pPage);
    }
    return pPage;
}