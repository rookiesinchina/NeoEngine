#pragma once

#include "Resource.h"


//A simple base class of all buffer classes for DirectX runtime. 

class Buffer : public Resource
{
public:
    Buffer(const std::wstring& bufferName = L"NoName");
    Buffer(const D3D12_RESOURCE_DESC& bufferDesc, const std::wstring& bufferName = L"NoName");
    Buffer(Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource, const std::wstring& bufferName = L"NoName");

    virtual~Buffer();

    virtual void SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue /* = nullptr */);

    virtual void CreateView(UINT numElements, UINT strideByteSize) = 0;
};