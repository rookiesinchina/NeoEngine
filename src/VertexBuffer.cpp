#include "VertexBuffer.h"

VertexBuffer::VertexBuffer(const std::wstring& vertexName)
    :Buffer(vertexName)
{};

VertexBuffer::~VertexBuffer()
{};

void VertexBuffer::CreateView(UINT NumVertice, UINT strideByteSize)
{
    if (NumVertice > 0)
    {
        m_VertexBufferView.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
        m_VertexBufferView.SizeInBytes = NumVertice * strideByteSize;
        m_VertexBufferView.StrideInBytes = strideByteSize;

        m_NumVertice = NumVertice;
        m_StrideByteSize = strideByteSize;
    }
}

void VertexBuffer::SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue)
{
    Buffer::SetD3D12Resource(d3d12Resource, nullptr);
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc)const
{
    throw std::exception("Error! Vertex buffer does not have shader resource view");
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc)const
{
    throw std::exception("Error! Vertex buffer does not have unordered access view");
}

