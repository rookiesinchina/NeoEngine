#pragma once

#include "Buffer.h"


class VertexBuffer : public Buffer
{
public:
    VertexBuffer(const std::wstring& vertexName = L"NoName");
    ~VertexBuffer();

    void SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue)override;

    //Create vertex buffer view for input vertice,this function should be invoked
    //after real d3d12resource has been created!
    //This function should only be invoked by CommandList class.
    void CreateView(UINT NumVertice, UINT strideByteSize)override;

    D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc /* = nullptr */)const override;
    D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc /* = nullptr */)const override;
    
    D3D12_VERTEX_BUFFER_VIEW GetVerterBufferView()const { return m_VertexBufferView; }

    UINT GetNumVertice()const { return m_NumVertice; }

    UINT GetVertexByteSize()const { return m_StrideByteSize; }
private:
    UINT m_NumVertice;
    UINT m_StrideByteSize;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
};