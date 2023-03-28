#pragma once

#include "d3dx12.h"
#include <wrl.h>
#include <memory>
#include <string>

//a wrapper class which includes ID3D12Resource() interface.
//Otherwise this class is also a Buffer or Texture base class

class Resource
{
public:
    explicit Resource(const std::wstring& name = L"NoName");
    Resource(const D3D12_RESOURCE_DESC& ResourceDesc, const std::wstring& ResourceName, const D3D12_CLEAR_VALUE* ClearValue = nullptr);
    Resource(Microsoft::WRL::ComPtr<ID3D12Resource> pResource, const std::wstring& ResourceName, const D3D12_CLEAR_VALUE* ClearValue = nullptr);
    virtual ~Resource();

    Resource(const Resource& copy);
    Resource& operator=(const Resource& assign);

    Resource(Resource&& move);
    Resource& operator=(Resource&& move);

    bool IsValidResource()const;

    const D3D12_RESOURCE_DESC GetD3D12ResourceDesc()const;

    Microsoft::WRL::ComPtr<ID3D12Resource> GetD3D12Resource()const;
    //Set D3D12Resource to Resource,this method does not to add resource state to ResourceBarrierTracker.
    virtual void SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue = nullptr);

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc = nullptr)const = 0;

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc = nullptr)const = 0;

    virtual void Reset();

    void SetName(const std::wstring& ResourceName);

    const D3D12_CLEAR_VALUE* GetClearValue()const { return m_d3d12ClearValue.get(); }

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_d3d12Resource;
    std::unique_ptr<D3D12_CLEAR_VALUE> m_d3d12ClearValue;
    std::wstring m_ResourceName;
};