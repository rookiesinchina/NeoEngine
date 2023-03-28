#include "Texture.h"
#include "Application.h"
#include "DescriptorAllocator.h"
#include "ResourceStateTracker.h"

Texture::Texture(TextureUsage textureUsage /* = TextureUsage::Albedo */, std::wstring textureName /* = "NoName" */)
    :Resource(textureName)
    , m_TextureUsage(textureUsage)
    , m_DescriptorAllocation()
    , m_IsRenderTarget(false)
    , m_IsDepthStencil(false)
{};

Texture::Texture(const D3D12_RESOURCE_DESC* pResourceDesc, const D3D12_CLEAR_VALUE* pClearValue /* = nullptr */, TextureUsage textureUsage /* = TextureUsage::Albedo */, std::wstring textureName /* = "NoName" */)
    :Resource(*pResourceDesc, textureName, pClearValue)
    , m_TextureUsage(textureUsage)
    , m_DescriptorAllocation()
    , m_IsRenderTarget(false)
    , m_IsDepthStencil(false)
{
    CreateView();
};

Texture::Texture(Microsoft::WRL::ComPtr<ID3D12Resource> pResource, const D3D12_CLEAR_VALUE* pClearValue /* = nullptr */, TextureUsage textureUsage /* = TextureUsage::Albedo */, std::wstring textureName /* = "NoName" */)
    :Resource(pResource, textureName, pClearValue)
    , m_TextureUsage(textureUsage)
    , m_DescriptorAllocation()
    , m_IsRenderTarget(false)
    , m_IsDepthStencil(false)
{
    CreateView();
};

Texture::Texture(const Texture& copy)
    :Resource::Resource(copy)
    , m_TextureUsage(copy.m_TextureUsage)
    , m_IsRenderTarget(copy.m_IsRenderTarget)
    , m_IsDepthStencil(copy.m_IsDepthStencil)
{
    CreateView();
}

Texture& Texture::operator=(const Texture& assign)
{
    if (this != &assign)
    {
        m_TextureUsage = assign.m_TextureUsage;
        m_IsRenderTarget = assign.m_IsRenderTarget;
        m_IsDepthStencil = assign.m_IsDepthStencil;


        Resource::operator=(assign);

        CreateView();
    }
    return *this;
}

Texture::Texture(Texture&& move)
    :Resource(move)
    , m_TextureUsage(move.m_TextureUsage)
    , m_IsRenderTarget(move.m_IsRenderTarget)
    , m_IsDepthStencil(move.m_IsDepthStencil)
{
    CreateView();
}

Texture& Texture::operator=(Texture&& move)
{
    if (this != &move)
    {
        m_TextureUsage = move.m_TextureUsage;
        m_IsRenderTarget = move.m_IsRenderTarget;
        m_IsDepthStencil = move.m_IsDepthStencil;


        Resource::operator=(move);

        CreateView();
    }
    return *this;
}

void Texture::SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue /* = nullptr */)
{
    Resource::SetD3D12Resource(d3d12Resource, ClearValue);
    CreateView();
}

void Texture::CreateView()
{
    if (m_d3d12Resource)
    {
        auto device = Application::GetApp()->GetDevice();

        auto ResourceDesc = m_d3d12Resource->GetDesc();

        auto flag = ResourceDesc.Flags;

        if ((flag & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0)
        {
            m_IsRenderTarget = true;
            m_DescriptorAllocation = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
            device->CreateRenderTargetView(m_d3d12Resource.Get(), nullptr, m_DescriptorAllocation.GetDescriptorHandle());
        }
        if ((flag & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
        {
            m_IsDepthStencil = true;
            m_DescriptorAllocation = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
            device->CreateDepthStencilView(m_d3d12Resource.Get(), nullptr, m_DescriptorAllocation.GetDescriptorHandle());
        }
        //-----------------------------------------------------------------
        std::lock_guard<std::mutex> lock(m_ShderResourceViewsMutex);
        std::lock_guard<std::mutex> guard(m_UnorderedAccessViewMutex);
        //-----------------------------------------------------------------
        m_ShaderResourceViewMap.clear();
        m_UnorderedAccessViewMap.clear();
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRenderTargetView()const
{
    assert(m_IsRenderTarget && "This resource is not render target resource.");
    return m_DescriptorAllocation.GetDescriptorHandle();
}    

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDepthStencilView()const
{
    assert(m_IsDepthStencil && "This resource is not depth stencil resource.");
    return m_DescriptorAllocation.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc)const
{
    UINT hash = 0;
    if (SrvDesc)
    {
        hash = std::hash<D3D12_SHADER_RESOURCE_VIEW_DESC>{}(*SrvDesc);
    }
    //--------------------------------------------------------------
    std::lock_guard<std::mutex> lock(m_ShderResourceViewsMutex);
    //--------------------------------------------------------------
    auto iter = m_ShaderResourceViewMap.find(hash);
    if (iter == m_ShaderResourceViewMap.end())
    {
        auto allocation = CreateShaderResourceView(SrvDesc);
        iter = m_ShaderResourceViewMap.insert({ hash,std::move(allocation) }).first;
    }

    return iter->second.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc)const
{
    UINT hash = 0;
    if (UavDesc)
    {
        hash = std::hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>{}(*UavDesc);
    }
    //------------------------------------------------------------------
    std::lock_guard<std::mutex> guard(m_UnorderedAccessViewMutex);
    //------------------------------------------------------------------
    auto iter = m_UnorderedAccessViewMap.find(hash);
    if (iter == m_UnorderedAccessViewMap.end())
    {
        auto allocation = CreateUnorderedAccessView(UavDesc);
        iter = m_UnorderedAccessViewMap.insert({ hash,std::move(allocation) }).first;
    }
    return iter->second.GetDescriptorHandle();
}

DescriptorAllocation Texture::CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc /* = nullptr */)const
{
    assert(m_d3d12Resource && "Resource has been released!");

    auto device = Application::GetApp()->GetDevice();
    auto allocation = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
    device->CreateShaderResourceView(m_d3d12Resource.Get(), SrvDesc, allocation.GetDescriptorHandle());
    return allocation;
}

DescriptorAllocation Texture::CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc /* = nullptr */)const
{
    assert(m_d3d12Resource && "Resource has been released!");

    auto device = Application::GetApp()->GetDevice();
    auto allocation = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
    device->CreateUnorderedAccessView(m_d3d12Resource.Get(), nullptr, UavDesc, allocation.GetDescriptorHandle());
    return allocation;
}

void Texture::Resize(UINT Width, UINT Height)
{
    if (m_d3d12Resource)
    {
        auto resourceDesc = m_d3d12Resource->GetDesc();
        if (resourceDesc.Width != Width || resourceDesc.Height != Height)
        {
            ResourceStateTracker::RemoveGlobalResourceState(m_d3d12Resource.Get());
            m_d3d12Resource.Reset();

            D3D12_RESOURCE_DESC newDesc = resourceDesc;
            newDesc.Width = Width;
            newDesc.Height = Height;

            auto device = Application::GetApp()->GetDevice();
            ThrowIfFailed(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &newDesc,
                D3D12_RESOURCE_STATE_COMMON,
                m_d3d12ClearValue.get(),
                IID_PPV_ARGS(&m_d3d12Resource)));
            ResourceStateTracker::AddGlobalResourceState(m_d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);

            CreateView();
        }
    }
}

void Texture::ResetFormat(DXGI_FORMAT Format)
{
    if (m_d3d12Resource)
    {
        auto resourceDesc = m_d3d12Resource->GetDesc();
        if (resourceDesc.Format != Format)
        {
            ResourceStateTracker::RemoveGlobalResourceState(m_d3d12Resource.Get());
            m_d3d12Resource.Reset();

            D3D12_RESOURCE_DESC newDesc = resourceDesc;
            resourceDesc.Format = Format;

            auto device = Application::GetApp()->GetDevice();
            ThrowIfFailed(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &newDesc,
                D3D12_RESOURCE_STATE_COMMON,
                m_d3d12ClearValue.get(),
                IID_PPV_ARGS(&m_d3d12Resource)));
            ResourceStateTracker::AddGlobalResourceState(m_d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);

            CreateView();
        }
    }
}

bool Texture::IsUavCompatibleFormat(DXGI_FORMAT Format)
{
    //@see:https://learn.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads
    //@see:https://stackoverflow.com/questions/64663216/how-can-i-resolve-dxgi-format-compatibility-issues
    auto device = Application::GetApp()->GetDevice();
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};

    switch (Format)
    {
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
        return true;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SINT:
        ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));
        return options.TypedUAVLoadAdditionalFormats;
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
        ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));
        if (options.TypedUAVLoadAdditionalFormats)
        {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = { Format,D3D12_FORMAT_SUPPORT1_NONE,D3D12_FORMAT_SUPPORT2_NONE };
            ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)));
            auto loadmask = D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD;
            auto storemask = D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
            if ((formatSupport.Support2 & loadmask) != 0 && (formatSupport.Support2 & storemask) != 0)
            {
                return true;
            }
            return false;
        }
    default:
        return false;
    }
}

bool Texture::IsBGRFormat(DXGI_FORMAT Format)
{
    switch (Format)
    {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

bool Texture::IsSRGBFormat(DXGI_FORMAT Format)
{
    switch (Format)
    {
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}