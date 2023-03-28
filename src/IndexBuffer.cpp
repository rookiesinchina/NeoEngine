#include "IndexBuffer.h"
#include <assert.h>

IndexBuffer::IndexBuffer(const std::wstring& indexName /* = L"NoName" */)
    :Buffer(indexName)
    ,m_NumIndex(0)
    ,m_IndexFormat(DXGI_FORMAT_UNKNOWN)
{};

void IndexBuffer::CreateView(UINT numIndex, UINT strideByteSize)
{
    assert(strideByteSize == 2 || strideByteSize == 4 && "Attention!StrideByteSize must be 2 or 4.");

    if (numIndex > 0)
    {
        m_IndexFormat = (strideByteSize == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        m_NumIndex = numIndex;
        
        m_IndexBufferView.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
        m_IndexBufferView.Format = m_IndexFormat;
        m_IndexBufferView.SizeInBytes = numIndex * strideByteSize;
    }
}

void IndexBuffer::SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue)
{
    Buffer::SetD3D12Resource(d3d12Resource, nullptr);
}

D3D12_CPU_DESCRIPTOR_HANDLE IndexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc)const
{
    throw std::exception("Error! Index buffer does not have shader resource view");
}

D3D12_CPU_DESCRIPTOR_HANDLE IndexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc)const
{
    throw std::exception("Error! Index buffer does not have unordered resource view");
}