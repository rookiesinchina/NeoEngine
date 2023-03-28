#pragma once

#include <d3d12.h>
#include <memory>
#include <array>

#include "Texture.h"
#include "RootSignature.h"
#include "FrustumCulling.h"
#include "Filter.h"
#include "GenerateSAT.h"

using namespace DirectX;

enum ShadowTechnology
{
    NoShadow,
    StandardShadowMap,
    VarianceShadowMap,
    SATVarianceShadowMapFP,
    SATVarianceShadowMapINT,
    CascadedShadowMap,
    CascadedVarianceShadowMap
};

//min and max filter size in texel size
const static int g_MinFilterSize = 1;
const static int g_MaxFilterSize = 10;



enum ShadowRootParameter
{
    ShadowConstantBuffer,
    ShadowPassBuffer,
    ShadowMaterialBuffer,
    ShadowAlphaTexture,   //note: we use the alpha channel of diffuse texture to do alpha test for shadow. 
    NumShadowRootParameter
};

struct ShadowPipelineState
{
    CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendDesc;
    CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL         DepthStencil;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargetFormats;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DepthStencilFormat;
    CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
    CD3DX12_PIPELINE_STATE_STREAM_GS                    GS;
    CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
    CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
    CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveType;
};

struct ShadowPassCb
{
    DirectX::XMFLOAT4X4 View;     //For point light,we draw six times for the whole scene and we use frustum culling to decrease hardware loading.
    DirectX::XMFLOAT4X4 Proj;
    DirectX::XMFLOAT4X4 ViewProj;
};


class Light;
class Scene;
class CommandList;
class RenderTarget;

class ShadowBase
{
public:
    ShadowBase(int width, int height, const Light* pLight, DXGI_FORMAT ShadowFormat, ShadowTechnology Technology);
    virtual ~ShadowBase() {};

    virtual void BeginShadow(std::shared_ptr<CommandList> commandList);

    virtual const Texture* GetShadow()const = 0;

    virtual void Resize(int newWidth, int newHeight) { m_Width = newWidth; m_Height = newHeight; };

    virtual bool SetFormat(DXGI_FORMAT Format) = 0;

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView()const = 0;

    
    //------------------------------------------------------------------------------------------
    DXGI_FORMAT GetFormat()const { return m_Format; }

    ShadowTechnology GetTechnology()const { return m_Technology; }
    //Get shadow pass buffer.For point light,use index to get buffer from different direction
    ShadowPassCb GetShadowPassBuffer(UINT index = 0)const { return m_ShadowPassBuffers[index]; }

    FrustumCullinger* GetFrustumCullinger()const { return m_pShadowFrustumCullinger.get(); }

    RootSignature* GetRootSignature()const { return m_pShadowRootSignature.get(); }
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState()const { return m_d3d12PipelineState; }

    void SetFilterSize(int FilterSize) { m_FilterSize = FilterSize; };

    ShadowTechnology GetShadowTechnology()const { return m_Technology; }
protected:
    std::unique_ptr<Texture> m_pShadowTexture;
    std::unique_ptr<Texture> m_pShadowDepthTexture;

    const Light* m_pLight;
    std::unique_ptr<FrustumCullinger> m_pShadowFrustumCullinger;

    int m_Width;
    int m_Height;
    DXGI_FORMAT m_Format;
    ShadowTechnology m_Technology;

    D3D12_VIEWPORT m_ShadowViewPort;
    RECT           m_ShadowScissorRect;

    std::vector<ShadowPassCb> m_ShadowPassBuffers;

    std::unique_ptr<RootSignature> m_pShadowRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12PipelineState;

    int m_FilterSize;
private:
    Microsoft::WRL::ComPtr<ID3DBlob> m_ShadowVS;
    Microsoft::WRL::ComPtr<ID3DBlob> m_ShadowPS;
  
};

/************************************************************************/
/*A class for standard shadow map                                       */
/************************************************************************/

enum StandardShadowMapFormat
{
    StandardLowPresionFormat = DXGI_FORMAT_R16_FLOAT,
    StandardHighPresionFormat =  DXGI_FORMAT_R32_FLOAT
};

class Shadow : public ShadowBase
{
public:
    Shadow(int width, int height, Light* pLight, DXGI_FORMAT format, ShadowTechnology Technology = StandardShadowMap);
    virtual ~Shadow();

    virtual void BeginShadow(std::shared_ptr<CommandList> commandList)override;

    virtual const Texture* GetShadow()const override;

    virtual bool SetFormat(DXGI_FORMAT Format);

    virtual void Resize(int newWidth, int newHeight)override;

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView()const { return  D3D12_CPU_DESCRIPTOR_HANDLE(); };

private:
    DescriptorAllocation m_PointRtvs;
};

/************************************************************************/
/* A class for variance shadow map                                      */
/************************************************************************/

enum VarianceShadowMapFormat
{
    VarianceLowPrecisionFormat = DXGI_FORMAT_R16G16_FLOAT,
    VarianceHighPrecisionFormat = DXGI_FORMAT_R32G32_FLOAT
};

class VarianceShadow : public ShadowBase
{
public:
    VarianceShadow(int width, int height, Light* pLight, DXGI_FORMAT format, ShadowTechnology Technology = VarianceShadowMap);
    virtual ~VarianceShadow() {};

    virtual void BeginShadow(std::shared_ptr<CommandList> commandList)override;

    virtual const Texture* GetShadow()const override;

    virtual bool SetFormat(DXGI_FORMAT Format) { return true; };

    virtual void Resize(int newWidth, int newHeight)override;

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView()const { return  D3D12_CPU_DESCRIPTOR_HANDLE(); };

    void SetFilterSize(FILTER_RADIUS Radius);
private:
    std::unique_ptr<Filter> m_pFilter;
    FILTER_RADIUS m_FilterSize;
    DescriptorAllocation m_PointRtvs;
};

/************************************************************************/
/* A class for SAT variance shadow map                                  */
/************************************************************************/

enum SATVarianceShadowMapFPFormat
{
    SATVarianceLowPrecisionFpFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
    SATVarianceHighPrecisionFpFormat = DXGI_FORMAT_R32G32B32A32_FLOAT
};

enum SATVarianceShadowMapINTFormat
{
    SATVarianceLowPrecisionIntFormat = DXGI_FORMAT_R16G16_UINT,
    SATVarianceHightPrecisionIntFormat = DXGI_FORMAT_R32G32_UINT
};

class  SATVarianceShadow : public VarianceShadow
{
public:
    SATVarianceShadow(int width, int height, Light* pLight, DXGI_FORMAT format,ShadowTechnology Technology);
    virtual ~SATVarianceShadow() {};

    virtual void BeginShadow(std::shared_ptr<CommandList> commandList)override;

    virtual const Texture* GetShadow()const override;

    virtual bool SetFormat(DXGI_FORMAT Format) { return true; };

    virtual void Resize(int newWidth, int newHeight)override;

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView()const { return  D3D12_CPU_DESCRIPTOR_HANDLE(); };
private:
    std::unique_ptr<GenerateSAT> m_pGenerateSAT;
};

/************************************************************************/
/*A class for cascaded shadow map                                       */
/************************************************************************/
//Note:the cascaded shadow map only support directional light for now.
enum CascadedShadowMapFormat
{
    CascadedLowPrecisionFormat = DXGI_FORMAT_R16_FLOAT,
    CascadedHighPrecisionFormat = DXGI_FORMAT_R32_FLOAT
};

#define MAX_CASCADES 8

const static XMVECTOR gHalfVector = { 0.5f,0.5f,0.5f,1.0f };
const static XMVECTOR gVectorZToZero = { 1.0f,1.0f,0.0f,1.0f };

class CascadedShadow :public ShadowBase
{
public:
    //Cascaded partation level - most for eight
    enum class CASCADED_LEVEL
    {
        CAS_LEVEL1 = 1,
        CAS_LEVEL2 = 2,
        CAS_LEVEL3 = 3,
        CAS_LEVEL4 = 4,
        CAS_LEVEL5 = 5,
        CAS_LEVEL6 = 6,
        CAS_LEVEL7 = 7,
        CAS_LEVEL8 = 8
    };
    //
    enum class FIT_PROJECTION_TO_CASCADES
    {
        FIT_TO_SCENE,
        FIT_TO_CASCADE
    };

    enum class FIT_TO_NEAR_FAR
    {
        FIT_TO_SCENE_AABB,
        FIT_TO_CASCADE_AABB
    };

public:
    CascadedShadow(int width, int height, Light* pLight,const Camera* pMainCamera ,DXGI_FORMAT format,CASCADED_LEVEL Level,FIT_PROJECTION_TO_CASCADES FitMethod,FIT_TO_NEAR_FAR FitNearFarMethod);
    virtual ~CascadedShadow() {};

    virtual void BeginShadow(std::shared_ptr<CommandList> commandList)override;

    virtual const Texture* GetShadow()const override { return nullptr; };

    virtual bool SetFormat(DXGI_FORMAT Format) { return true; };

    virtual void Resize(int newWidth, int newHeight)override {};

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView()const { return  D3D12_CPU_DESCRIPTOR_HANDLE(); };

    void SetCacadedLevel(CASCADED_LEVEL Level);

    void SetCascadedFitMethod(FIT_PROJECTION_TO_CASCADES Method);

    void SetCascadedFitNearFarMethod(FIT_TO_NEAR_FAR Method);

protected:
    struct Triangle
    {
        XMVECTOR pt[3];
        bool culled;
    };

    void UpdateShadowInfo();

    void ComputeCascadePartitionFactor();

    void ComputeNearAndFar(FLOAT& fNearPlane, FLOAT& fFarPlane, DirectX::FXMVECTOR vLightCameraOrthographicMin,
        DirectX::FXMVECTOR vLightCameraOrthographicMax, DirectX::XMVECTOR* pvPointsInCameraView);

    void GetFrustumPointsViewSpaceFromInterval(float FrustumClipStart, float FrustumClipEnd,
        DirectX::XMMATRIX EyeProjection, DirectX::XMVECTOR* FrustumPointsViewSpace);

private:
    CASCADED_LEVEL             m_CascadeLevel;
    FIT_PROJECTION_TO_CASCADES m_SelectedCascadedFit;
    FIT_TO_NEAR_FAR            m_SelectedCascadedNearFar;

    std::unique_ptr<Texture> m_pShadowTexture;
    DescriptorAllocation     m_CascadedRenderTargetDescriptors;

    const Camera* m_pMainCamera;
    //The cascaded partition factors- 0 ---- 1
    std::array<float, MAX_CASCADES> m_CascadePartitionFactor;
    std::array<float, MAX_CASCADES> m_CascadePartitionDepth;
    //some cameras as sub cameras of light camera.
    //This can do frustum culling for us.
    std::array<std::unique_ptr<Camera>, MAX_CASCADES > m_CascadedCameras;

    //following variables are for variance / sat variance shadow map
    std::unique_ptr<Filter> m_pFilter;
    std::unique_ptr<GenerateSAT> m_pGenerateSAT;
};

/************************************************************************/
/*A class for cascaded variance shadow map                              */
/************************************************************************/
//Note:the cascaded shadow map only support directional light for now.

enum CascadedVarianceShadowMapFormat
{
    CascadedVarianceLowPrecisionFormat      = DXGI_FORMAT_R16G16_FLOAT,
    CascadedVarianceHighPrecisionFormat     = DXGI_FORMAT_R32G32_FLOAT,
    CascadedVarianceLowPrecisionFormatFP    = DXGI_FORMAT_R16G16B16A16_FLOAT,
    CascadedVarianceHighPrecisionFormatFP   = DXGI_FORMAT_R32G32B32A32_FLOAT,
    CascadedVarianceLowPrecisionFormatINT   = DXGI_FORMAT_R16G16_UINT,
    CascadedVarianceHighPrecisionFormatINT  = DXGI_FORMAT_R32G32_UINT
};

class CascadedVarianceShadow : public ShadowBase
{
public:
    //Cascaded partation level - most for eight
    enum class CASCADED_LEVEL
    {
        CAS_LEVEL1 = 1,
        CAS_LEVEL2 = 2,
        CAS_LEVEL3 = 3,
        CAS_LEVEL4 = 4,
        CAS_LEVEL5 = 5,
        CAS_LEVEL6 = 6,
        CAS_LEVEL7 = 7,
        CAS_LEVEL8 = 8
    };
    //
    enum class FIT_PROJECTION_TO_CASCADES
    {
        FIT_TO_SCENE,
        FIT_TO_CASCADE
    };

    enum class FIT_TO_NEAR_FAR
    {
        FIT_TO_SCENE_AABB,
        FIT_TO_CASCADE_AABB
    };

public:
    CascadedVarianceShadow(int width, int height, Light* pLight, const Camera* pMainCamera, DXGI_FORMAT format, 
        CASCADED_LEVEL Level, FIT_PROJECTION_TO_CASCADES FitMethod, FIT_TO_NEAR_FAR FitNearFarMethod);
    virtual ~CascadedVarianceShadow() {};

    virtual void BeginShadow(std::shared_ptr<CommandList> commandList)override;

    virtual const Texture* GetShadow()const override { return nullptr; };

    virtual bool SetFormat(DXGI_FORMAT Format) { return true; };

    virtual void Resize(int newWidth, int newHeight)override {};

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView()const { return  D3D12_CPU_DESCRIPTOR_HANDLE(); };

    void SetCacadedLevel(CASCADED_LEVEL Level);

    void SetCascadedFitMethod(FIT_PROJECTION_TO_CASCADES Method);

    void SetCascadedFitNearFarMethod(FIT_TO_NEAR_FAR Method);

protected:
    struct Triangle
    {
        XMVECTOR pt[3];
        bool culled;
    };

    void UpdateShadowInfo();

    void ComputeCascadePartitionFactor();

    void ComputeNearAndFar(FLOAT& fNearPlane, FLOAT& fFarPlane, DirectX::FXMVECTOR vLightCameraOrthographicMin,
        DirectX::FXMVECTOR vLightCameraOrthographicMax, DirectX::XMVECTOR* pvPointsInCameraView);

    void GetFrustumPointsViewSpaceFromInterval(float FrustumClipStart, float FrustumClipEnd,
        DirectX::XMMATRIX EyeProjection, DirectX::XMVECTOR* FrustumPointsViewSpace);

private:
    CASCADED_LEVEL             m_CascadeLevel;
    FIT_PROJECTION_TO_CASCADES m_SelectedCascadedFit;
    FIT_TO_NEAR_FAR            m_SelectedCascadedNearFar;

    std::unique_ptr<Texture> m_pShadowTexture;
    DescriptorAllocation     m_CascadedRenderTargetDescriptors;

    const Camera* m_pMainCamera;
    //The cascaded partition factors- 0 ---- 1
    std::array<float, MAX_CASCADES> m_CascadePartitionFactor;
    std::array<float, MAX_CASCADES> m_CascadePartitionDepth;
    //some cameras as sub cameras of light camera.
    //This can do frustum culling for us.
    std::array<std::unique_ptr<Camera>, MAX_CASCADES> m_CascadedCameras;

    //following variables are for variance / sat variance shadow map
    std::unique_ptr<Filter> m_pFilter;
    std::unique_ptr<GenerateSAT> m_pGenerateSAT;
};