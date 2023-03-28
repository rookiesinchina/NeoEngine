#pragma once

#include "Buffer.h"

class IndexBuffer : public Buffer
{
public:
    IndexBuffer(const std::wstring& indexName = L"NoName");
    ~IndexBuffer() {};
   
    void SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue)override;
    //Create a index buffer view for input index.
    //Note: @param strideByteSize represent index format.If this parameter is 2,that means index format is R16_UINT.And if parameter is 4,that means index format is R32_UINT.
    //This function shoule be invoked in CommandList class.
    void CreateView(UINT numIndex, UINT strideByteSize)override;

    D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc /* = nullptr */)const override;

    D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc /* = nullptr */)const override;

    DXGI_FORMAT GetIndexBufferFormat()const { return m_IndexFormat; }

    UINT GetNumIndex()const { return m_NumIndex; }

    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView()const { return m_IndexBufferView; }
protected:

private:
    DXGI_FORMAT m_IndexFormat;
    UINT m_NumIndex;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
};