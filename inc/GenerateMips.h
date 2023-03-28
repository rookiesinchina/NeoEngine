#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "DescriptorAllocation.h"


class RootSignature;

struct GenerateMipsCB
{
    uint32_t numMips;
    uint32_t SrcDimension;          //a state value for the width and height of source texture is odd or even.
    uint32_t SrcMipLevel;
    uint32_t Padding;
    DirectX::XMFLOAT2 TexelSize;
};

namespace GenerateMipsRoot
{
    enum RootParameter
    {
        GenerateMipsCB,
        Src,
        Mips,
        NumRootParameters
    };
}

//A class for generating mips for textures
class GenerateMips
{
public:
    GenerateMips();
    ~GenerateMips();

    const RootSignature* GetRootSignature()const { return m_ComputeRootSignature.get(); }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState()const { return m_d3d12ComputePipelineState; }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDefaultUAV()const { return m_DefaultUAV.GetDescriptorHandle(); }
private:
    std::unique_ptr<RootSignature> m_ComputeRootSignature;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12ComputePipelineState;
    //
    DescriptorAllocation m_DefaultUAV;
};
