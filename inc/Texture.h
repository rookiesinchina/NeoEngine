#pragma once


#include "Resource.h"
#include "d3dUtil.h"
#include "DescriptorAllocation.h"
#include <mutex>


//Note: this a simple version of class Texture
class Texture : public Resource
{
public:
    explicit Texture(TextureUsage textureUsage = TextureUsage::Diffuse, std::wstring textureName = L"NoName");
    explicit Texture(
        const D3D12_RESOURCE_DESC* pResourceDesc,
        const D3D12_CLEAR_VALUE* pClearValue = nullptr,
        TextureUsage textureUsage = TextureUsage::Diffuse,
        std::wstring textureName = L"NoName");
    explicit Texture(
        Microsoft::WRL::ComPtr<ID3D12Resource> pResource,
        const D3D12_CLEAR_VALUE* pClearValue = nullptr,
        TextureUsage textureUsage = TextureUsage::Diffuse,
        std::wstring textureName = L"NoName");

    Texture(const Texture& copy);
    Texture(Texture&& move);

    Texture& operator=(const Texture& assign);
    Texture& operator=(Texture&& move);

    ~Texture() {};

    void SetTextureUsage(TextureUsage textureUsage) { m_TextureUsage = textureUsage; };

    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView()const;

    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView()const;
    //A simplified method to get SRV in Cpu.
    D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc /* = nullptr */)const override;
    //A simplified method to get UAV in Cpu.
    D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc /* = nullptr */)const override;
    /**
     * If some texture use Texture(TextureUsage,wstring) construction function for simple initilization
     * then we should use this function to initiliza totally.
     */
    void SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue = nullptr)override;

    void Resize(UINT Width, UINT Height);

    void ResetFormat(DXGI_FORMAT Format);

    //Check if a given format is UAV compatible format. 
    static bool IsUavCompatibleFormat(DXGI_FORMAT Format);

    static bool IsBGRFormat(DXGI_FORMAT Format);

    static bool IsSRGBFormat(DXGI_FORMAT Format);

protected:
    void CreateView();

    DescriptorAllocation CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* SrvDesc = nullptr)const;
    DescriptorAllocation CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* UavDesc = nullptr)const;
private:
    TextureUsage m_TextureUsage;
    //Descriptor for a texture which may be rendertarget or depthstencil.
    bool m_IsRenderTarget;
    bool m_IsDepthStencil;
    DescriptorAllocation m_DescriptorAllocation;

    mutable std::unordered_map<UINT,DescriptorAllocation> m_ShaderResourceViewMap;
    mutable std::unordered_map<UINT,DescriptorAllocation> m_UnorderedAccessViewMap;

    mutable std::mutex m_ShderResourceViewsMutex;
    mutable std::mutex m_UnorderedAccessViewMutex;
};