#pragma once

#include <DirectXMath.h>
#include <memory>
#include <wrl.h>
#include "d3dx12.h"
#include "DescriptorAllocation.h"


enum class FILTER_RADIUS
{
    FILTER_RADIUS_2     = 2,
    FILTER_RADIUS_4     = 4,
    FILTER_RADIUS_8     = 8,
    FILTER_RADIUS_16    = 16,
    FILTER_RADIUS_32    = 32,
    FILTER_RADIUS_64    = 64
};

enum class FILTER_TYPE
{
    FILTER_BOX,
    FILTER_GAUSSIAN
};

enum FilterRootParameters
{
    FilterConstantBuffer,
    FilterTexture,
    NumFilterRootParameters
};


/**
 * Here we just use two weights for all filter radius.
 * We can do this since we can downsample texture to decrease filter radius.
 * And we use GPU hardware to bilinear sample for decrease sample times.
 @see: https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
 */
struct FilterConstant
{
    DirectX::XMFLOAT2 Weights;
    DirectX::XMFLOAT2 Offsets;
    DirectX::XMFLOAT2 InputTextureSize;
    UINT              TextureArraySize;
    DirectX::XMUINT3  Padding;
};


class Texture;
class RootSignature;
class CommandList;

//@brief: a class for filter technology - (box and gaussian),the filter use fragment shader to execute filtering.
class Filter
{
public:
    Filter(const Texture* pInputTexture, FILTER_RADIUS Radius,FILTER_TYPE Type);
    ~Filter() {};

    void BeginFilter(std::shared_ptr<CommandList> commandList);

    const Texture* GetFilterTexture()const;

    void SetFilterRadius(FILTER_RADIUS Radius);

protected:
    /**
     * For better performance,sometimes we will use large filter kernel,this will degenerate performance.
     * we use a better approximation method which is use a smaller texture as render target and use bilinear sample to get result.
     * @see:https://kalogirou.net/2006/05/20/how-to-do-good-bloom-for-hdr-rendering/
     */
    float CalculateOutputTextureScalingFactor();

    void Resize();
protected:
    //Input texture
    const Texture* m_pTexture;
    //Filter constant buffer
    FilterConstant m_FilterConstant;
    //two ping-pong textures for filtering.
    std::unique_ptr<Texture> m_pPingPongTexture0;
    std::unique_ptr<Texture> m_pPingPongTexture1;
    //the final texture which has same size with input texture.
    std::unique_ptr<Texture> m_pResultTexture;
    //filter root signature and pipeline state.
    std::unique_ptr<RootSignature> m_pRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12SamplePipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12FilterHPipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12FilterVPipelineState;
    //filter radius.
    FILTER_RADIUS m_Radius;
    //descriptors for texture array
    DescriptorAllocation m_RtvDescriptors;
    //Viewport and scissor rect
    D3D12_VIEWPORT m_ViewPort;
    RECT           m_ScissorRect;
    //Filter type
    FILTER_TYPE m_FilterType;
};