#include "Resource.h"
#include "d3dUtil.h"
#include "Application.h"
#include "ResourceStateTracker.h"

Resource::Resource(const std::wstring& name)
    :m_ResourceName(name)
    , m_d3d12Resource(nullptr)
    , m_d3d12ClearValue(nullptr)
{};

Resource::~Resource() {};

Resource::Resource(const D3D12_RESOURCE_DESC& ResourceDesc, const std::wstring& ResourceName, const D3D12_CLEAR_VALUE* ClearValue)
    :m_ResourceName(ResourceName)
{
    D3D12_RESOURCE_DESC resDesc = ResourceDesc;
    if ((resDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0)
    {
        std::wstring error = L"Error!The resource (Name: " + ResourceName + L") has render target flag must set valid clear value!";
        assert(ClearValue && error.c_str());
    }

    m_d3d12ClearValue = nullptr;
    if (ClearValue != nullptr)
    {
        m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*ClearValue);
    }

    auto device = Application::GetApp()->GetDevice();

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        m_d3d12ClearValue.get(),
        IID_PPV_ARGS(&m_d3d12Resource)));

    //Add resource inilization state to resource barrier tracker.
    ResourceStateTracker::AddGlobalResourceState(m_d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);
}

Resource::Resource(Microsoft::WRL::ComPtr<ID3D12Resource> pResource, const std::wstring& ResourceName, const D3D12_CLEAR_VALUE* ClearValue /* = nullptr */)
    :m_d3d12Resource(pResource)
{
    if (ClearValue)
    {
        m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*ClearValue);
    }
    SetName(ResourceName);
}

Resource::Resource(const Resource& copy)
    : m_d3d12Resource(copy.m_d3d12Resource)
    , m_ResourceName(copy.m_ResourceName)
{
    m_d3d12ClearValue = nullptr;
    if (copy.m_d3d12ClearValue)
    {
        m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*copy.m_d3d12ClearValue);
    }
}

Resource& Resource::operator=(const Resource& assign)
{
    if (this != &assign)
    {
        m_d3d12ClearValue = nullptr;
        if (assign.m_d3d12ClearValue)
        {
            m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*assign.m_d3d12ClearValue);
        }

        m_d3d12Resource = assign.m_d3d12Resource;
        m_ResourceName = assign.m_ResourceName;
    }

    return *this;
}

Resource::Resource(Resource&& move)
    :m_d3d12Resource(std::move(move.m_d3d12Resource))
    , m_ResourceName(std::move(move.m_ResourceName))
    , m_d3d12ClearValue(std::move(move.m_d3d12ClearValue))
{
    move.m_d3d12Resource = nullptr;
    move.m_d3d12ClearValue = nullptr;
    move.m_ResourceName.clear();
};

Resource& Resource::operator=(Resource&& move)
{
    if (this != &move)
    {
        m_d3d12Resource = std::move(move.m_d3d12Resource);
        m_ResourceName = std::move(move.m_ResourceName);
        m_d3d12ClearValue = std::move(move.m_d3d12ClearValue);

        move.m_d3d12Resource = nullptr;
        move.m_d3d12ClearValue = nullptr;
        move.m_ResourceName.clear();
    }
    return *this;
}

bool Resource::IsValidResource()const
{
    return m_d3d12Resource != nullptr;
}

const D3D12_RESOURCE_DESC Resource::GetD3D12ResourceDesc()const
{
    assert(IsValidResource() && "Invalid Resource");

    D3D12_RESOURCE_DESC resDesc;
    resDesc = m_d3d12Resource->GetDesc();
    return resDesc;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Resource::GetD3D12Resource()const
{
    return m_d3d12Resource;
}

void Resource::SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue)
{
    assert(d3d12Resource.Get() != nullptr && "Invalid Resource");

    m_d3d12Resource = d3d12Resource;
    m_d3d12ClearValue = nullptr;
    if (ClearValue)
    {
        m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*ClearValue);
    }
}

void Resource::Reset()
{
    m_d3d12Resource.Reset();
    m_d3d12ClearValue.reset();
    m_ResourceName = L"NoName";

    m_d3d12Resource = nullptr;
    m_d3d12ClearValue = nullptr;
}

void Resource::SetName(const std::wstring& ResourceName)
{
    m_ResourceName = ResourceName;
}