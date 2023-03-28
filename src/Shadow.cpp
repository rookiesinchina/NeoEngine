#pragma once

#include "Shadow.h"
#include "d3dx12.h"
#include "Application.h"
#include "Scene.h"
#include "RenderTarget.h"
#include "Light.h"
#include "CommandList.h"
#include "Pass.h"

#include <DirectXColors.h>

ShadowBase::ShadowBase(int width, int height,const Light* pLight, DXGI_FORMAT ShadowFormat, ShadowTechnology Technology) 
    :m_Width(width),
    m_Height(height),
    m_pLight(pLight),
    m_Format(ShadowFormat),
    m_Technology(Technology),
    m_pShadowTexture(nullptr),
    m_pShadowDepthTexture(nullptr),
    m_pShadowFrustumCullinger(std::make_unique<FrustumCullinger>())
{
    //Check format valid
    switch (m_Technology)
    {
    case StandardShadowMap:
    case CascadedShadowMap:
        assert(m_Format == DXGI_FORMAT_R16_FLOAT || m_Format == DXGI_FORMAT_R32_FLOAT);
        break;
    case VarianceShadowMap:
        assert(m_Format == DXGI_FORMAT_R16G16_FLOAT || m_Format == DXGI_FORMAT_R32G32_FLOAT);
        break;
    case SATVarianceShadowMapFP:
        assert(m_Format == DXGI_FORMAT_R16G16B16A16_FLOAT || m_Format == DXGI_FORMAT_R32G32B32A32_FLOAT);
        break;
    case SATVarianceShadowMapINT:
        assert(m_Format == DXGI_FORMAT_R16G16_UINT || m_Format == DXGI_FORMAT_R32G32_UINT);
        break;
    case CascadedVarianceShadowMap:
        assert(m_Format == DXGI_FORMAT_R16G16_FLOAT ||
            m_Format == DXGI_FORMAT_R32G32_FLOAT ||
            m_Format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
            m_Format == DXGI_FORMAT_R32G32B32A32_FLOAT ||
            m_Format == DXGI_FORMAT_R16G16_UINT ||
            m_Format == DXGI_FORMAT_R32G32_UINT);
        break;
    }

    //
    m_ShadowViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, m_Width, m_Height);
    m_ShadowScissorRect = { 0,0,m_Width,m_Height };
    //Create depth stencil for all shadow technology
    D3D12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D24_UNORM_S8_UINT, m_Width, m_Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE DepthClear = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
    m_pShadowDepthTexture = std::make_unique<Texture>(&depthDesc, &DepthClear, TextureUsage::Depth, L"Shadow Depth");
    //Create root signature for all shadow technology
    D3D12_FEATURE_DATA_ROOT_SIGNATURE rootVersion = {};
    rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(Application::GetApp()->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootVersion, sizeof(rootVersion))))
    {
        rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    //we use the alpha channel of diffuse texture of object to do alpha-test.
    CD3DX12_DESCRIPTOR_RANGE1 alphaRange = {};
    alphaRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_MaxTextureNum, 1, 0);

    CD3DX12_ROOT_PARAMETER1 shadowRootParameter[ShadowRootParameter::NumShadowRootParameter];
    shadowRootParameter[ShadowRootParameter::ShadowConstantBuffer].InitAsConstantBufferView(0);
    shadowRootParameter[ShadowRootParameter::ShadowPassBuffer].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE);
    shadowRootParameter[ShadowRootParameter::ShadowMaterialBuffer].InitAsShaderResourceView(0);
    shadowRootParameter[ShadowRootParameter::ShadowAlphaTexture].InitAsDescriptorTable(1, &alphaRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC anisotropicLinear = {};
    anisotropicLinear.Init(0);

    D3D12_ROOT_SIGNATURE_FLAGS Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc = {};
    versionRootSignatureDesc.Init_1_1(ShadowRootParameter::NumShadowRootParameter, shadowRootParameter, 1, &anisotropicLinear, Flags);

    m_pShadowRootSignature = std::make_unique<RootSignature>(versionRootSignatureDesc.Desc_1_1, rootVersion.HighestVersion);
    //then,we create pipeline state

    switch (m_Technology)
    {
    case StandardShadowMap:
    case CascadedShadowMap:
        m_ShadowVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", nullptr, "VS", "vs_5_1");
        m_ShadowPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", nullptr, "PS", "ps_5_1");
        break;
    case VarianceShadowMap:
    {
        D3D_SHADER_MACRO macro[] =
        {
            "STANDARD_VSM","1",
            NULL,NULL
        };
        m_ShadowVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", nullptr, "VS", "vs_5_1");
        m_ShadowPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", macro, "PS", "ps_5_1");
    }
    break;
    case SATVarianceShadowMapFP:
    {
        D3D_SHADER_MACRO macro[] =
        {
            "SAT_VSM_FP","1",
            NULL,NULL
        };
        m_ShadowVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", nullptr, "VS", "vs_5_1");
        m_ShadowPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", macro, "PS", "ps_5_1");
    }
    break;
    case SATVarianceShadowMapINT:
    {
        D3D_SHADER_MACRO macro[] =
        {
            "SAT_VSM_INT","1",
            NULL,NULL
        };
        m_ShadowVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", nullptr, "VS", "vs_5_1");
        m_ShadowPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", macro, "PS", "ps_5_1");
    }
    break;
    case CascadedVarianceShadowMap:
    {
        //standard variance shadow map
        if (m_Format == DXGI_FORMAT_R16G16_FLOAT || m_Format == DXGI_FORMAT_R32G32_FLOAT)
        {
            D3D_SHADER_MACRO macro[] =
            {
                "STANDARD_VSM","1",
                NULL,NULL
            };
            m_ShadowVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", nullptr, "VS", "vs_5_1");
            m_ShadowPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", macro, "PS", "ps_5_1");
        }
        //variance shadow map fp
        if (m_Format == DXGI_FORMAT_R16G16B16A16_FLOAT || m_Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
        {
            D3D_SHADER_MACRO macro[] =
            {
                "SAT_VSM_FP","1",
                NULL,NULL
            };
            m_ShadowVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", nullptr, "VS", "vs_5_1");
            m_ShadowPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", macro, "PS", "ps_5_1");
        }
        //variance shadow map uint
        if (m_Format == DXGI_FORMAT_R16G16_UINT || m_Format == DXGI_FORMAT_R32G32_UINT)
        {
            D3D_SHADER_MACRO macro[] =
            {
                "SAT_VSM_INT","1",
                NULL,NULL
            };
            m_ShadowVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", nullptr, "VS", "vs_5_1");
            m_ShadowPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Shadow.hlsl", macro, "PS", "ps_5_1");
        }
    }
    break;
    default:
        assert(FALSE && "Error!Unexcepted shadow technology!");
        break;
    }

    D3D12_RT_FORMAT_ARRAY RtArray = {};
    RtArray.NumRenderTargets = 1;
    RtArray.RTFormats[0] = m_Format;

    CD3DX12_RASTERIZER_DESC rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    //For standard shadow map,we need a bias to solve shadow acne problem.
    //But for variance shadow map,the bias is not necessary any more.
    if (m_Technology == StandardShadowMap || m_Technology == CascadedShadowMap)
    {
        rasterizer.DepthBiasClamp = 0.01f;
        rasterizer.SlopeScaledDepthBias = 1.0f;
        rasterizer.DepthBias = 100000;
    }

    ShadowPipelineState pipelinestate;
    pipelinestate.BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipelinestate.Rasterizer = rasterizer;
    pipelinestate.DepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipelinestate.RenderTargetFormats = RtArray;
    pipelinestate.DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipelinestate.VS = CD3DX12_SHADER_BYTECODE(m_ShadowVS.Get());
    pipelinestate.PS = CD3DX12_SHADER_BYTECODE(m_ShadowPS.Get());
    pipelinestate.pRootSignature = m_pShadowRootSignature->GetRootSignature().Get();
    pipelinestate.InputLayout = { ModelSpace::ModelInputElemets,_countof(ModelSpace::ModelInputElemets) };
    pipelinestate.SampleDesc = { 1,0 };
    pipelinestate.PrimitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStreamDesc = { sizeof(pipelinestate), &pipelinestate };

    ThrowIfFailed(Application::GetApp()->GetDevice()->CreatePipelineState(&pipelineStreamDesc, IID_PPV_ARGS(&m_d3d12PipelineState)));
}

void ShadowBase::BeginShadow(std::shared_ptr<CommandList> commandList)
{
    //update shadow pass
    if (m_pLight->GetLightType() == LightType::Directional || m_pLight->GetLightType() == LightType::Spot)
    {
        m_ShadowPassBuffers.resize(1);
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].View, DirectX::XMMatrixTranspose(m_pLight->GetLightViewMatrix()));
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].Proj, DirectX::XMMatrixTranspose(m_pLight->GetLightProjMatrix()));
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].ViewProj, DirectX::XMMatrixTranspose(m_pLight->GetLightViewMatrix() * m_pLight->GetLightProjMatrix()));
    }
    else if (m_pLight->GetLightType() == LightType::Point)
    {
        m_ShadowPassBuffers.resize(6);
        for (int i = 0; i < 6; ++i)
        {
            DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[i].View, DirectX::XMMatrixTranspose(m_pLight->GetLightViewMatrix(i)));
            DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[i].Proj, DirectX::XMMatrixTranspose(m_pLight->GetLightProjMatrix()));
            DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[i].ViewProj, DirectX::XMMatrixTranspose(m_pLight->GetLightViewMatrix(i) * m_pLight->GetLightProjMatrix()));
        }

    }
}


/************************************************************************/
/*Following functions are for standard shadow map                       */
/************************************************************************/

Shadow::Shadow(int width, int height,Light* pLight,DXGI_FORMAT format,ShadowTechnology Technology)
    :ShadowBase(width, height,pLight ,format, Technology)
{
    //According to light type to change shadow map size
    UINT16 arraySize = pLight->GetLightType() == LightType::Point ? 6 : 1;
    //Firstly,we create shadow map texture.
    D3D12_RESOURCE_DESC shadowDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_Format, m_Width, m_Height, arraySize, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    //Note:here clear value must be 1.0f.Unless depth value will be wrong.
    D3D12_CLEAR_VALUE shadowClear = CD3DX12_CLEAR_VALUE(m_Format, DirectX::Colors::White);
    m_pShadowTexture = std::make_unique<Texture>(&shadowDesc, &shadowClear, TextureUsage::RenderTargetTexture, L"Standard Shadow Map");
    //Create Rtv for shadow texture.
    //Note:here we see texture2D as a special case of texture2DArray.
    m_PointRtvs = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, arraySize);
    for (int i = 0; i < arraySize; ++i)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_Format;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.PlaneSlice = 0;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

        Application::GetApp()->GetDevice()->CreateRenderTargetView(m_pShadowTexture->GetD3D12Resource().Get(), &rtvDesc, m_PointRtvs.GetDescriptorHandle(i));
    }
}

Shadow::~Shadow() {};

void Shadow::BeginShadow(std::shared_ptr<CommandList> commandList)
{
    ShadowBase::BeginShadow(commandList);
    //
    assert(m_pShadowTexture && "Shadow Texture has not been initialized!");
    assert(m_pShadowDepthTexture && "Shadow Depth Texture has not been initialized!");

    commandList->SetD3D12ViewPort(&m_ShadowViewPort);
    commandList->SetD3D12ScissorRect(&m_ShadowScissorRect);
    commandList->BarrierTransition(m_pShadowTexture.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->BarrierTransition(m_pShadowDepthTexture.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    //here we render all objects arraysize times for different light view.
    for (int i = 0; i < m_pShadowTexture->GetD3D12ResourceDesc().DepthOrArraySize; ++i)
    {
        {
            commandList->GetGraphicsCommandList2()->ClearRenderTargetView(m_PointRtvs.GetDescriptorHandle(i), DirectX::Colors::White, 0, nullptr);
            commandList->GetGraphicsCommandList2()->ClearDepthStencilView(m_pShadowDepthTexture->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            commandList->GetGraphicsCommandList2()->OMSetRenderTargets(1, &m_PointRtvs.GetDescriptorHandle(i), TRUE, &m_pShadowDepthTexture->GetDepthStencilView());
        }
        m_pShadowFrustumCullinger->BindFrustumCamera(m_pLight->GetLightCamera(i));
        commandList->RenderShadow(this, i);
    }
}

void Shadow::Resize(int newWidth, int newHeight)
{
    if (newWidth != m_Width || newHeight != m_Height)
    {
        ShadowBase::Resize(newWidth, newHeight);

        m_pShadowTexture->Resize(newWidth, newHeight);
        m_pShadowDepthTexture->Resize(newWidth, newHeight);

        m_ShadowViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, m_Width, m_Height);
        m_ShadowScissorRect = { 0,0,m_Width,m_Height };
    }
}

const Texture* Shadow::GetShadow()const
{
    assert(m_pShadowTexture && "Shadow texture has not been created!");
    return m_pShadowTexture.get();
}

bool Shadow::SetFormat(DXGI_FORMAT Format)
{
    return true;
}

/************************************************************************/
/* Following functions are for variance shadow map                      */
/************************************************************************/

VarianceShadow::VarianceShadow(int width, int height, Light* pLight, DXGI_FORMAT format,ShadowTechnology Technology)
    : ShadowBase(width, height, pLight, format, Technology)
{
    //According to light type to change shadow map size
    UINT16 arraySize = pLight->GetLightType() == LightType::Point ? 6 : 1;
    //Firstly,we create shadow map texture.
    D3D12_RESOURCE_DESC shadowDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_Format, m_Width, m_Height, arraySize, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    //Note:here clear value must be 1.0f.Unless depth value will be wrong.
    D3D12_CLEAR_VALUE shadowClear = CD3DX12_CLEAR_VALUE(m_Format, DirectX::Colors::White);
    //But for SAT variance shadow map,it will use different clear value
    //For detail,please see shader about shadow.And this is very important for SAT
    if (Technology == SATVarianceShadowMapFP)
    {
        shadowClear.Color[0] = 0.5f;
        shadowClear.Color[1] = 0.5f;
        shadowClear.Color[2] = 0.0f;
        shadowClear.Color[3] = 0.0f;
    }
    else if (Technology == SATVarianceShadowMapINT)
    {
        shadowClear.Color[0] = 262144.0f;
        shadowClear.Color[1] = 262144.0f;
    }
    else//for variance shadow map,the clear value is 1.0f.
    {
        shadowClear.Color[0] = 1.0f;
        shadowClear.Color[1] = 1.0f;
    }
    
    m_pShadowTexture = std::make_unique<Texture>(&shadowDesc, &shadowClear, TextureUsage::RenderTargetTexture, L"Variance Shadow Map");
    if (Technology == VarianceShadowMap)
    {
        m_pFilter = std::make_unique<Filter>(m_pShadowTexture.get(), FILTER_RADIUS::FILTER_RADIUS_4, FILTER_TYPE::FILTER_BOX);
    }
    //Create Rtv for shadow texture.
    //Note:here we see texture2D as a special case of texture2DArray.
    m_PointRtvs = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, arraySize);
    for (int i = 0; i < arraySize; ++i)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_Format;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.PlaneSlice = 0;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

        Application::GetApp()->GetDevice()->CreateRenderTargetView(m_pShadowTexture->GetD3D12Resource().Get(), &rtvDesc, m_PointRtvs.GetDescriptorHandle(i));
    }
}

void VarianceShadow::BeginShadow(std::shared_ptr<CommandList> commandList)
{
    ShadowBase::BeginShadow(commandList);
    //
    assert(m_pShadowTexture && "Shadow Texture has not been initialized!");
    assert(m_pShadowDepthTexture && "Shadow Depth Texture has not been initialized!");
    //Set different clear value for SAT variance shadow.
    float clear[4];
    if (m_Technology == SATVarianceShadowMapFP)
    {
        clear[0] = 0.5f;
        clear[1] = 0.5f;
        clear[2] = 0.0f;
        clear[3] = 0.0f;
    }
    else if (m_Technology == SATVarianceShadowMapINT)
    {
        clear[0] = 262144.0f;
        clear[1] = 262144.0f;
        clear[2] = 0.0f;
        clear[3] = 0.0f;
    }
    else//for variance shadow map,the clear value is 1.0f.
    {
        clear[0] = 1.0f;
        clear[1] = 1.0f;
        clear[2] = 0.0f;
        clear[3] = 0.0f;
    }
    //Render shadow.
    commandList->SetD3D12ViewPort(&m_ShadowViewPort);
    commandList->SetD3D12ScissorRect(&m_ShadowScissorRect);
    commandList->BarrierTransition(m_pShadowTexture.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->BarrierTransition(m_pShadowDepthTexture.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    //here we render all objects six times for different light view.
    for (int i = 0; i < m_pShadowTexture->GetD3D12ResourceDesc().DepthOrArraySize; ++i)
    {
        {
            commandList->GetGraphicsCommandList2()->ClearRenderTargetView(m_PointRtvs.GetDescriptorHandle(i), clear, 0, nullptr);
            commandList->GetGraphicsCommandList2()->ClearDepthStencilView(m_pShadowDepthTexture->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            commandList->GetGraphicsCommandList2()->OMSetRenderTargets(1, &m_PointRtvs.GetDescriptorHandle(i), TRUE, &m_pShadowDepthTexture->GetDepthStencilView());
        }
        m_pShadowFrustumCullinger->BindFrustumCamera(m_pLight->GetLightCamera(i));
        commandList->RenderShadow(this, i);
    }
    //for variance shadow map,after rendering depth,we can use box-filter to blur it for soft shadow.
    //Note:only the standard variance shadow map needs to blur.
    if (m_Technology == VarianceShadowMap)
    {
        m_pFilter->BeginFilter(commandList);
    }
}

const Texture* VarianceShadow::GetShadow()const
{
    assert(m_pShadowTexture && "Shadow texture has not been created!");
    //we need to blur for standard variance shadow map.
    return m_pFilter->GetFilterTexture();
}

void VarianceShadow::Resize(int newWidth, int newHeight)
{
    if (newWidth != m_Width || newHeight != m_Height)
    {
        ShadowBase::Resize(newWidth, newHeight);

        m_pShadowTexture->Resize(newWidth, newHeight);
        m_pShadowDepthTexture->Resize(newWidth, newHeight);

        m_ShadowViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, m_Width, m_Height);
        m_ShadowScissorRect = { 0,0,m_Width,m_Height };
    }
}

void VarianceShadow::SetFilterSize(FILTER_RADIUS Radius)
{
    m_pFilter->SetFilterRadius(Radius);
}

/************************************************************************/
/* Following function is for SAT variance shadow map                    */
/************************************************************************/

SATVarianceShadow::SATVarianceShadow(int width, int height, Light* pLight, DXGI_FORMAT format, ShadowTechnology Technology)
    :VarianceShadow(width, height, pLight, format,Technology)
{
    m_pGenerateSAT = std::make_unique<GenerateSAT>(m_pShadowTexture.get());
}

void SATVarianceShadow::BeginShadow(std::shared_ptr<CommandList> commandList)
{
    VarianceShadow::BeginShadow(commandList);
    //We use depth to generate SAT.
    m_pGenerateSAT->GenerateSATs(commandList);
}

const Texture* SATVarianceShadow::GetShadow()const
{
    return m_pGenerateSAT->GetSATs();
}

void SATVarianceShadow::Resize(int newWidth, int newHeight)
{
    VarianceShadow::Resize(newWidth, newHeight);
}
/************************************************************************/
/* Following function is for cascaded shadow map                        */
/************************************************************************/

CascadedShadow::CascadedShadow(int width, int height, Light* pLight,const Camera* pMainCamera ,DXGI_FORMAT format,
    CASCADED_LEVEL Level,FIT_PROJECTION_TO_CASCADES FitMethod,FIT_TO_NEAR_FAR FitNearFarMethod)
    :ShadowBase(width, height, pLight, format, ShadowTechnology::CascadedShadowMap)
    ,m_CascadeLevel(Level)
    ,m_SelectedCascadedFit(FitMethod)
    ,m_SelectedCascadedNearFar(FitNearFarMethod)
    ,m_pMainCamera(pMainCamera)
{
    assert(pLight->GetLightType() == Directional && "Error!The cascaded shadow map only can be used for directional light for now");
    assert(m_pMainCamera && "Error!The cascaded shadow map need main camera,but the main camera is null!");
    //Create cascaded shadow map texture
    UINT16 arraySize = (UINT16)m_CascadeLevel;
    D3D12_RESOURCE_DESC shadowDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_Format, m_Width, m_Height, arraySize, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_CLEAR_VALUE ShadowClear = CD3DX12_CLEAR_VALUE(m_Format, DirectX::Colors::White);
    m_pShadowTexture = std::make_unique<Texture>(&shadowDesc, &ShadowClear, TextureUsage::RenderTargetTexture, L"Cascaded Shadow Map Texture Array");
    if (m_Technology == CascadedVarianceShadowMap)
    {
        //for standrad variance shadow map,we need to blur it.
        if (m_Format == DXGI_FORMAT_R16G16_FLOAT || m_Format == DXGI_FORMAT_R32G32_FLOAT)
        {
            m_pFilter = std::make_unique<Filter>(m_pShadowTexture.get(), FILTER_RADIUS::FILTER_RADIUS_4, FILTER_TYPE::FILTER_BOX);
        }
        //for sat variance shadow map,we need to generate sat.
        if (m_Format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
            m_Format == DXGI_FORMAT_R32G32B32A32_FLOAT ||
            m_Format == DXGI_FORMAT_R16G16_UINT ||
            m_Format == DXGI_FORMAT_R32G32_UINT)
        {
            m_pGenerateSAT = std::make_unique<GenerateSAT>(m_pShadowTexture.get());
        }
    }

    //Create render targets for texture array
    m_CascadedRenderTargetDescriptors = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, arraySize);
    for (UINT16 i = 0; i < arraySize; ++i)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_Format;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.PlaneSlice = 0;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

        Application::GetApp()->GetDevice()->CreateRenderTargetView(
            m_pShadowTexture->GetD3D12Resource().Get(), &rtvDesc, m_CascadedRenderTargetDescriptors.GetDescriptorHandle(i));
    }
    //Create sub cameras for light camera
    for (int i = 0; i < (int)m_CascadeLevel; ++i)
    {
        m_CascadedCameras[i] = std::make_unique<Camera>(Orthographic);
        m_pLight->GetLightCamera()->SetSubCameras(m_CascadedCameras[i].get(), 1);
    }
    //Compute cascaded factors
    ComputeCascadePartitionFactor();
}

void CascadedShadow::BeginShadow(std::shared_ptr<CommandList> commandList)
{
    //Before rendering,we need to update buffer firstly
    UpdateShadowInfo();
    //
    m_ShadowPassBuffers.resize(1);
    for (int i = 0; i < (int)m_CascadeLevel; ++i)
    {
        //set constant buffer for each cascade.
        auto LightViewMatrix = m_pLight->GetLightViewMatrix();
        //Use sub cameras of light camera to get proj matrix.
        auto LightProjMatrix = m_CascadedCameras[i]->GetProj();
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].View, DirectX::XMMatrixTranspose(LightViewMatrix));
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].Proj, DirectX::XMMatrixTranspose(LightProjMatrix));
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].ViewProj, DirectX::XMMatrixTranspose(LightViewMatrix * LightProjMatrix));
        
        float clear[4];
        clear[0] = 1.0f;
        clear[1] = 1.0f;
        clear[2] = 0.0f;
        clear[3] = 0.0f;
        //Clear render target and depth stencil
        {
            commandList->SetD3D12ViewPort(&m_ShadowViewPort);
            commandList->SetD3D12ScissorRect(&m_ShadowScissorRect);
            commandList->ClearRenderTargetTexture(m_pShadowTexture.get(), clear);
            commandList->ClearDepthStencilTexture(m_pShadowDepthTexture.get(), D3D12_CLEAR_FLAG_DEPTH);
            commandList->GetGraphicsCommandList2()->OMSetRenderTargets(1, &m_CascadedRenderTargetDescriptors.GetDescriptorHandle(i), 
                FALSE, &m_pShadowDepthTexture->GetDepthStencilView());
        }
        //Here we use sub cameras to do frustum culling.
        m_pShadowFrustumCullinger->BindFrustumCamera(m_CascadedCameras[i].get());
        commandList->RenderShadow(this);
    }

    //for variance shadow map,after rendering depth,we can use box-filter to blur it for soft shadow.
    //Note:only the standard variance shadow map needs to blur.
    if (m_Format == DXGI_FORMAT_R16G16_FLOAT || m_Format == DXGI_FORMAT_R32G32_FLOAT)
    {
        m_pFilter->BeginFilter(commandList);
    }
    //generate sat.
    if (m_Format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
        m_Format == DXGI_FORMAT_R32G32B32A32_FLOAT ||
        m_Format == DXGI_FORMAT_R16G16_UINT ||
        m_Format == DXGI_FORMAT_R32G32_UINT)
    {
        m_pGenerateSAT->GenerateSATs(commandList);
    }
}

void CascadedShadow::ComputeCascadePartitionFactor()
{
    switch (m_CascadeLevel)
    {
    case CascadedShadow::CASCADED_LEVEL::CAS_LEVEL1:
        m_CascadePartitionFactor[0] = 1.0f;
        break;
    case CascadedShadow::CASCADED_LEVEL::CAS_LEVEL2:
        m_CascadePartitionFactor[0] = 0.4f;
        m_CascadePartitionFactor[1] = 1.0f;
        break;
    case CascadedShadow::CASCADED_LEVEL::CAS_LEVEL3:
        m_CascadePartitionFactor[0] = 0.2f;
        m_CascadePartitionFactor[1] = 0.6f;
        m_CascadePartitionFactor[2] = 1.0f;
        break;
    case CascadedShadow::CASCADED_LEVEL::CAS_LEVEL4:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.3f;
        m_CascadePartitionFactor[2] = 0.6f;
        m_CascadePartitionFactor[3] = 1.0f;
        break;
    case CascadedShadow::CASCADED_LEVEL::CAS_LEVEL5:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.25f;
        m_CascadePartitionFactor[2] = 0.4f;
        m_CascadePartitionFactor[3] = 0.6f;
        m_CascadePartitionFactor[4] = 1.0f;
        break;
    case CascadedShadow::CASCADED_LEVEL::CAS_LEVEL6:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.2f;
        m_CascadePartitionFactor[2] = 0.35f;
        m_CascadePartitionFactor[3] = 0.5f;
        m_CascadePartitionFactor[4] = 0.75f;
        m_CascadePartitionFactor[5] = 1.0f;
        break;
    case CascadedShadow::CASCADED_LEVEL::CAS_LEVEL7:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.2f;
        m_CascadePartitionFactor[2] = 0.35f;
        m_CascadePartitionFactor[3] = 0.5f;
        m_CascadePartitionFactor[4] = 0.65f;
        m_CascadePartitionFactor[5] = 0.8f;
        m_CascadePartitionFactor[6] = 1.0f;
        break;
    case CascadedShadow::CASCADED_LEVEL::CAS_LEVEL8:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.2f;
        m_CascadePartitionFactor[2] = 0.3f;
        m_CascadePartitionFactor[3] = 0.4f;
        m_CascadePartitionFactor[4] = 0.55f;
        m_CascadePartitionFactor[5] = 0.7f;
        m_CascadePartitionFactor[6] = 0.85f;
        m_CascadePartitionFactor[7] = 1.0f;
        break;
    }
}

void CascadedShadow::UpdateShadowInfo()
{
    assert(m_pMainCamera && "Error!Main camera is null!");
    //Firstly,we need to divide the view space of main camera
    float DepthRange = m_pMainCamera->GetFarZ() - m_pMainCamera->GetNearZ();
    //we loop to compute proj matrix for each cascade
    for (int i = 0; i < (int)m_CascadeLevel; ++i)
    {
        float cascadeDepthStart = 0.0f;
        float cascadeDepthEnd = 0.0f;
        //According to partition mathod to compute slice
        if (m_SelectedCascadedFit == FIT_PROJECTION_TO_CASCADES::FIT_TO_SCENE)
        {
            cascadeDepthStart = 0.0f;
            cascadeDepthEnd = DepthRange * m_CascadePartitionFactor[i];
        }
        else if (m_SelectedCascadedFit == FIT_PROJECTION_TO_CASCADES::FIT_TO_CASCADE)
        {
            if (i == 0) cascadeDepthStart = 0.0f;
            else        cascadeDepthStart = DepthRange * m_CascadePartitionFactor[i - 1];
            cascadeDepthEnd = DepthRange * m_CascadePartitionFactor[i];
        }
        m_CascadePartitionDepth[i] = cascadeDepthEnd;
        //then we use start and end value to compute slice view frustum points
        DirectX::XMVECTOR FrustumCornersViewSpace[8];
        GetFrustumPointsViewSpaceFromInterval(cascadeDepthStart, cascadeDepthEnd, m_pMainCamera->GetProj(), FrustumCornersViewSpace);
        //next,we need to transform these points to light space
        DirectX::XMMATRIX ViewToWorld = m_pMainCamera->GetInvView();
        DirectX::XMMATRIX WorldToLight = m_pLight->GetLightViewMatrix();
        DirectX::XMVECTOR FrustumCornersWorldSpace[8];
        DirectX::XMVECTOR FrustumCornersLightSpace[8];
        DirectX::XMVECTOR FrustumSliceMaxLightSpace = { -FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX };
        DirectX::XMVECTOR FrustumSliceMinLightSpace = { FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX };
        //then,we use these points to compute min and max,and to compute x and y for light frustum.
        for (int i = 0; i < 8; ++i)
        {
            FrustumCornersWorldSpace[i] = DirectX::XMVector3TransformCoord(FrustumCornersViewSpace[i], ViewToWorld);
            FrustumCornersLightSpace[i] = DirectX::XMVector3TransformCoord(FrustumCornersViewSpace[i], ViewToWorld * WorldToLight);
            FrustumSliceMaxLightSpace = DirectX::XMVectorMax(FrustumSliceMaxLightSpace, FrustumCornersLightSpace[i]);
            FrustumSliceMinLightSpace = DirectX::XMVectorMin(FrustumSliceMinLightSpace, FrustumCornersLightSpace[i]);
        }
        //
        DirectX::XMVECTOR WorldUnitsPerTexelVector;
        if (m_SelectedCascadedFit == FIT_PROJECTION_TO_CASCADES::FIT_TO_SCENE)
        {
            DirectX::XMVECTOR Diagonal = DirectX::XMVectorSubtract(
                FrustumCornersWorldSpace[7],
                FrustumCornersWorldSpace[0]);
            Diagonal = DirectX::XMVector3Length(Diagonal);

            XMVECTOR Offset =
                (Diagonal - (FrustumSliceMaxLightSpace - FrustumSliceMinLightSpace)) * gHalfVector;
            Offset *= gVectorZToZero;

            FrustumSliceMaxLightSpace += Offset;
            FrustumSliceMinLightSpace -= Offset;

            float CascadedLength = DirectX::XMVectorGetX(Diagonal);
            float WorldUnitsPerTexel = (CascadedLength / (float)m_Width);
            WorldUnitsPerTexelVector = DirectX::XMVectorSet(WorldUnitsPerTexel, WorldUnitsPerTexel, 0.0f, 0.0f);
        }
        else//fllowing code , I can not understand.......
        {
            // We calculate a looser bound based on the size of the PCF blur.  This ensures us that we're 
            // sampling within the correct map.
            float fScaleDuetoBlureAMT = ((float)(m_FilterSize * 2 + 1)
                / (float)m_Width);
            XMVECTORF32 vScaleDuetoBlureAMT = { fScaleDuetoBlureAMT, fScaleDuetoBlureAMT, 0.0f, 0.0f };

            float fNormalizeByBufferSize = (1.0f / (float)m_Width);
            XMVECTOR vNormalizeByBufferSize = DirectX::XMVectorSet(fNormalizeByBufferSize, fNormalizeByBufferSize, 0.0f, 0.0f);

            // We calculate the offsets as a percentage of the bound.
            XMVECTOR vBoarderOffset = FrustumSliceMaxLightSpace - FrustumSliceMinLightSpace;
            vBoarderOffset *= gHalfVector;
            vBoarderOffset *= vScaleDuetoBlureAMT;
            FrustumSliceMaxLightSpace += vBoarderOffset;
            FrustumSliceMinLightSpace -= vBoarderOffset;

            // The world units per texel are used to snap  the orthographic projection
            // to texel sized increments.  
            // Because we're fitting tighly to the cascades, the shimmering shadow edges will still be present when the 
            // camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
            WorldUnitsPerTexelVector = FrustumSliceMaxLightSpace - FrustumSliceMinLightSpace;
            WorldUnitsPerTexelVector *= vNormalizeByBufferSize;
        }
        //Here we limit camera move footstep into one texel to solve shadows jitter.
        //This is a matter of integer dividing by the world space size of a texel
        FrustumSliceMinLightSpace /= WorldUnitsPerTexelVector;
        FrustumSliceMinLightSpace = XMVectorFloor(FrustumSliceMinLightSpace);
        FrustumSliceMinLightSpace *= WorldUnitsPerTexelVector;

        FrustumSliceMaxLightSpace /= WorldUnitsPerTexelVector;
        FrustumSliceMaxLightSpace = XMVectorFloor(FrustumSliceMaxLightSpace);
        FrustumSliceMaxLightSpace *= WorldUnitsPerTexelVector;
        //Finally,we can use scene AABB and frustum slice to compute znear and zfar of light frustum.
        //Here,we have two method to decide znear and zfar.
        //FIT_TO_SCENE: directly use scene AABB to compute znear and zfar.This method is enough for many cases.
        //But in special case,such as the camera view is small but scene aabb is long,this will lead the light frustum has a large distance between znear and zfar.
        //FIT_TO_CASCADE: use camera frustum slice and scene aabb to compute znear and zfar.This will get a tighter light frustum.
        //for cascaded shadow map,this is a very important technology,since many slice will be smaller than scene aabb.
        
        //We need to transform the corners of scene AABB to light space
        auto SceneAABBWorldSpace = Scene::GetScene()->GetSceneBoundingBox();
        DirectX::XMFLOAT3 AABBCornersWorldSpace[8];
        SceneAABBWorldSpace.GetCorners(AABBCornersWorldSpace);
        DirectX::XMVECTOR AABBCornerLightSpace[8];
        for (int i = 0; i < 8; ++i)
        {
            AABBCornerLightSpace[i] = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&AABBCornersWorldSpace[i]), m_pLight->GetLightViewMatrix());
        }
        //A simple method.
        //We just compute the min and max of aabb in light space.
        float zNear = 0.0f;
        float zFar = 1000.0f;
        if (m_SelectedCascadedNearFar == FIT_TO_NEAR_FAR::FIT_TO_SCENE_AABB)
        {
            DirectX::XMVECTOR MaxAABBCornersLightSpace = { -FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX };
            DirectX::XMVECTOR MinAABBCornersLightSpace = { FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX };
            //compute min and max
            for (int i = 0; i < 8; ++i)
            {
                MaxAABBCornersLightSpace = DirectX::XMVectorMax(MaxAABBCornersLightSpace, AABBCornerLightSpace[i]);
                MinAABBCornersLightSpace = DirectX::XMVectorMin(MinAABBCornersLightSpace, AABBCornerLightSpace[i]);
            }
            zNear = DirectX::XMVectorGetZ(MinAABBCornersLightSpace);
            zFar = DirectX::XMVectorGetZ(MaxAABBCornersLightSpace);
        }
        //A better method to compute tighter frustum for light.
        else if (m_SelectedCascadedNearFar == FIT_TO_NEAR_FAR::FIT_TO_CASCADE_AABB)
        {
            ComputeNearAndFar(zNear, zFar, FrustumSliceMinLightSpace, FrustumSliceMaxLightSpace, AABBCornerLightSpace);
        }
        //Until now,we get all parameter for light frustum,we can compute light frustum.
        m_CascadedCameras[i]->SetLens(
            DirectX::XMVectorGetX(FrustumSliceMinLightSpace),
            DirectX::XMVectorGetX(FrustumSliceMaxLightSpace),
            DirectX::XMVectorGetY(FrustumSliceMinLightSpace),
            DirectX::XMVectorGetY(FrustumSliceMaxLightSpace),
            zNear,
            zFar);
    }
}


void CascadedShadow::GetFrustumPointsViewSpaceFromInterval(
    float FrustumClipStart,
    float FrustumClipEnd,
    DirectX::XMMATRIX EyeProjection,
    DirectX::XMVECTOR* FrustumPointsViewSpace)
{
    DirectX::BoundingFrustum FrustumClip;
    DirectX::BoundingFrustum::CreateFromMatrix(FrustumClip, EyeProjection);
    FrustumClip.Near = FrustumClipStart;
    FrustumClip.Far = FrustumClipEnd;

    //Points Layout
    //    Near        Far
    //  0------1    4------5
    //  |      |    |      |  
    //  |      |    |      |
    //  2------3    6------7

    DirectX::XMVECTOR LeftTopSlope = { FrustumClip.LeftSlope,FrustumClip.TopSlope,1.0f,0.0f };
    DirectX::XMVECTOR RightTopSlope = { FrustumClip.RightSlope,FrustumClip.TopSlope,1.0f,0.0f };
    DirectX::XMVECTOR LeftBottomSlope = { FrustumClip.LeftSlope,FrustumClip.BottomSlope,1.0f,0.0f };
    DirectX::XMVECTOR RightBottomSlope = { FrustumClip.RightSlope,FrustumClip.BottomSlope,1.0f,0.0f };

    DirectX::XMVECTOR Near = DirectX::XMVectorReplicate(FrustumClip.Near);
    DirectX::XMVECTOR Far = DirectX::XMVectorReplicate(FrustumClip.Far);

    DirectX::XMVECTOR Points[8];
    //Near
    Points[0] = DirectX::XMVectorMultiply(LeftTopSlope, Near);
    Points[1] = DirectX::XMVectorMultiply(RightTopSlope, Near);
    Points[2] = DirectX::XMVectorMultiply(LeftBottomSlope, Near);
    Points[3] = DirectX::XMVectorMultiply(RightBottomSlope, Near);
    //Far
    Points[4] = DirectX::XMVectorMultiply(LeftTopSlope, Far);
    Points[5] = DirectX::XMVectorMultiply(RightTopSlope, Far);
    Points[6] = DirectX::XMVectorMultiply(LeftBottomSlope, Far);
    Points[7] = DirectX::XMVectorMultiply(RightBottomSlope, Far);

    for (int i = 0; i < 8; ++i)
    {
        FrustumPointsViewSpace[i] = DirectX::XMVectorAdd(
            DirectX::XMVector3Rotate(Points[i], DirectX::XMLoadFloat4(&FrustumClip.Orientation)),
            DirectX::XMLoadFloat3(&FrustumClip.Origin));
    }
}

void CascadedShadow::ComputeNearAndFar(
    FLOAT& fNearPlane,
    FLOAT& fFarPlane,
    DirectX::FXMVECTOR vLightCameraOrthographicMin,
    DirectX::FXMVECTOR vLightCameraOrthographicMax,
    DirectX::XMVECTOR* pvPointsInCameraView)
{
    // Initialize the near and far planes
    fNearPlane = FLT_MAX;
    fFarPlane = -FLT_MAX;

    Triangle triangleList[16];
    INT iTriangleCnt = 1;

    triangleList[0].pt[0] = pvPointsInCameraView[0];
    triangleList[0].pt[1] = pvPointsInCameraView[1];
    triangleList[0].pt[2] = pvPointsInCameraView[2];
    triangleList[0].culled = false;

    // These are the indices used to tesselate an AABB into a list of triangles.
    static const INT iAABBTriIndexes[] =
    {
        0,1,2,  0,2,3,
        4,5,6,  4,6,7,
        0,4,7,  0,3,7,
        1,6,5,  1,2,6,
        0,1,4,  1,4,5,
        2,3,6,  3,6,7
    };

    INT iPointPassesCollision[3];

    // At a high level: 
    // 1. Iterate over all 12 triangles of the AABB.  
    // 2. Clip the triangles against each plane. Create new triangles as needed.
    // 3. Find the min and max z values as the near and far plane.

    //This is easier because the triangles are in camera spacing making the collisions tests simple comparisions.

    float fLightCameraOrthographicMinX = DirectX::XMVectorGetX(vLightCameraOrthographicMin);
    float fLightCameraOrthographicMaxX = DirectX::XMVectorGetX(vLightCameraOrthographicMax);
    float fLightCameraOrthographicMinY = DirectX::XMVectorGetY(vLightCameraOrthographicMin);
    float fLightCameraOrthographicMaxY = DirectX::XMVectorGetY(vLightCameraOrthographicMax);

    for (INT AABBTriIter = 0; AABBTriIter < 12; ++AABBTriIter)
    {

        triangleList[0].pt[0] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 0]];
        triangleList[0].pt[1] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 1]];
        triangleList[0].pt[2] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 2]];
        iTriangleCnt = 1;
        triangleList[0].culled = FALSE;

        // Clip each invidual triangle against the 4 frustums.  When ever a triangle is clipped into new triangles, 
        //add them to the list.
        for (INT frustumPlaneIter = 0; frustumPlaneIter < 4; ++frustumPlaneIter)
        {

            FLOAT fEdge;
            INT iComponent;

            if (frustumPlaneIter == 0)
            {
                fEdge = fLightCameraOrthographicMinX; // todo make float temp
                iComponent = 0;
            }
            else if (frustumPlaneIter == 1)
            {
                fEdge = fLightCameraOrthographicMaxX;
                iComponent = 0;
            }
            else if (frustumPlaneIter == 2)
            {
                fEdge = fLightCameraOrthographicMinY;
                iComponent = 1;
            }
            else
            {
                fEdge = fLightCameraOrthographicMaxY;
                iComponent = 1;
            }

            for (INT triIter = 0; triIter < iTriangleCnt; ++triIter)
            {
                // We don't delete triangles, so we skip those that have been culled.
                if (!triangleList[triIter].culled)
                {
                    INT iInsideVertCount = 0;
                    DirectX::XMVECTOR tempOrder;
                    // Test against the correct frustum plane.
                    // This could be written more compactly, but it would be harder to understand.

                    if (frustumPlaneIter == 0)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (DirectX::XMVectorGetX(triangleList[triIter].pt[triPtIter]) >
                                DirectX::XMVectorGetX(vLightCameraOrthographicMin))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if (frustumPlaneIter == 1)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (DirectX::XMVectorGetX(triangleList[triIter].pt[triPtIter]) <
                                DirectX::XMVectorGetX(vLightCameraOrthographicMax))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if (frustumPlaneIter == 2)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (DirectX::XMVectorGetY(triangleList[triIter].pt[triPtIter]) >
                                DirectX::XMVectorGetY(vLightCameraOrthographicMin))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (DirectX::XMVectorGetY(triangleList[triIter].pt[triPtIter]) <
                                DirectX::XMVectorGetY(vLightCameraOrthographicMax))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }

                    // Move the points that pass the frustum test to the begining of the array.
                    if (iPointPassesCollision[1] && !iPointPassesCollision[0])
                    {
                        tempOrder = triangleList[triIter].pt[0];
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;
                        iPointPassesCollision[1] = FALSE;
                    }
                    if (iPointPassesCollision[2] && !iPointPassesCollision[1])
                    {
                        tempOrder = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter].pt[2];
                        triangleList[triIter].pt[2] = tempOrder;
                        iPointPassesCollision[1] = TRUE;
                        iPointPassesCollision[2] = FALSE;
                    }
                    if (iPointPassesCollision[1] && !iPointPassesCollision[0])
                    {
                        tempOrder = triangleList[triIter].pt[0];
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;
                        iPointPassesCollision[1] = FALSE;
                    }

                    if (iInsideVertCount == 0)
                    { // All points failed. We're done,  
                        triangleList[triIter].culled = true;
                    }
                    else if (iInsideVertCount == 1)
                    {// One point passed. Clip the triangle against the Frustum plane
                        triangleList[triIter].culled = false;

                        // 
                        DirectX::XMVECTOR vVert0ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[0];
                        DirectX::XMVECTOR vVert0ToVert2 = triangleList[triIter].pt[2] - triangleList[triIter].pt[0];

                        // Find the collision ratio.
                        FLOAT fHitPointTimeRatio = fEdge - DirectX::XMVectorGetByIndex(triangleList[triIter].pt[0], iComponent);
                        // Calculate the distance along the vector as ratio of the hit ratio to the component.
                        FLOAT fDistanceAlongVector01 = fHitPointTimeRatio / DirectX::XMVectorGetByIndex(vVert0ToVert1, iComponent);
                        FLOAT fDistanceAlongVector02 = fHitPointTimeRatio / DirectX::XMVectorGetByIndex(vVert0ToVert2, iComponent);
                        // Add the point plus a percentage of the vector.
                        vVert0ToVert1 *= fDistanceAlongVector01;
                        vVert0ToVert1 += triangleList[triIter].pt[0];
                        vVert0ToVert2 *= fDistanceAlongVector02;
                        vVert0ToVert2 += triangleList[triIter].pt[0];

                        triangleList[triIter].pt[1] = vVert0ToVert2;
                        triangleList[triIter].pt[2] = vVert0ToVert1;

                    }
                    else if (iInsideVertCount == 2)
                    { // 2 in  // tesselate into 2 triangles


                        // Copy the triangle\(if it exists) after the current triangle out of
                        // the way so we can override it with the new triangle we're inserting.
                        triangleList[iTriangleCnt] = triangleList[triIter + 1];

                        triangleList[triIter].culled = false;
                        triangleList[triIter + 1].culled = false;

                        // Get the vector from the outside point into the 2 inside points.
                        XMVECTOR vVert2ToVert0 = triangleList[triIter].pt[0] - triangleList[triIter].pt[2];
                        XMVECTOR vVert2ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[2];

                        // Get the hit point ratio.
                        FLOAT fHitPointTime_2_0 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
                        FLOAT fDistanceAlongVector_2_0 = fHitPointTime_2_0 / XMVectorGetByIndex(vVert2ToVert0, iComponent);
                        // Calcaulte the new vert by adding the percentage of the vector plus point 2.
                        vVert2ToVert0 *= fDistanceAlongVector_2_0;
                        vVert2ToVert0 += triangleList[triIter].pt[2];

                        // Add a new triangle.
                        triangleList[triIter + 1].pt[0] = triangleList[triIter].pt[0];
                        triangleList[triIter + 1].pt[1] = triangleList[triIter].pt[1];
                        triangleList[triIter + 1].pt[2] = vVert2ToVert0;

                        //Get the hit point ratio.
                        FLOAT fHitPointTime_2_1 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
                        FLOAT fDistanceAlongVector_2_1 = fHitPointTime_2_1 / XMVectorGetByIndex(vVert2ToVert1, iComponent);
                        vVert2ToVert1 *= fDistanceAlongVector_2_1;
                        vVert2ToVert1 += triangleList[triIter].pt[2];
                        triangleList[triIter].pt[0] = triangleList[triIter + 1].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter + 1].pt[2];
                        triangleList[triIter].pt[2] = vVert2ToVert1;
                        // Cncrement triangle count and skip the triangle we just inserted.
                        ++iTriangleCnt;
                        ++triIter;


                    }
                    else
                    { // all in
                        triangleList[triIter].culled = false;

                    }
                }// end if !culled loop            
            }
        }
        for (INT index = 0; index < iTriangleCnt; ++index)
        {
            if (!triangleList[index].culled)
            {
                // Set the near and far plan and the min and max z values respectivly.
                for (int vertind = 0; vertind < 3; ++vertind)
                {
                    float fTriangleCoordZ = XMVectorGetZ(triangleList[index].pt[vertind]);
                    if (fNearPlane > fTriangleCoordZ)
                    {
                        fNearPlane = fTriangleCoordZ;
                    }
                    if (fFarPlane < fTriangleCoordZ)
                    {
                        fFarPlane = fTriangleCoordZ;
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*Following function is for cascaded shadow map                         */
/************************************************************************/

CascadedVarianceShadow::CascadedVarianceShadow(int width, int height, Light* pLight, const Camera* pMainCamera, DXGI_FORMAT format,
    CASCADED_LEVEL Level, FIT_PROJECTION_TO_CASCADES FitMethod, FIT_TO_NEAR_FAR FitNearFarMethod)
    :ShadowBase(width, height, pLight, format, ShadowTechnology::CascadedVarianceShadowMap)
    , m_CascadeLevel(Level)
    , m_SelectedCascadedFit(FitMethod)
    , m_SelectedCascadedNearFar(FitNearFarMethod)
    , m_pMainCamera(pMainCamera)
{
    assert(pLight->GetLightType() == Directional && "Error!The cascaded shadow map only can be used for directional light for now");
    assert(m_pMainCamera && "Error!The cascaded shadow map need main camera,but the main camera is null!");
    //Create cascaded shadow map texture
    UINT16 arraySize = (UINT16)m_CascadeLevel;
    D3D12_RESOURCE_DESC shadowDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_Format, m_Width, m_Height, arraySize, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_CLEAR_VALUE ShadowClear = CD3DX12_CLEAR_VALUE(m_Format, DirectX::Colors::White);
    m_pShadowTexture = std::make_unique<Texture>(&shadowDesc, &ShadowClear, TextureUsage::RenderTargetTexture, L"Cascaded Shadow Map Texture Array");

    //for standrad variance shadow map,we need to blur it.
    if (m_Format == DXGI_FORMAT_R16G16_FLOAT || m_Format == DXGI_FORMAT_R32G32_FLOAT)
    {
        m_pFilter = std::make_unique<Filter>(m_pShadowTexture.get(), FILTER_RADIUS::FILTER_RADIUS_4, FILTER_TYPE::FILTER_BOX);
    }
    //for sat variance shadow map,we need to generate sat.
    if (m_Format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
        m_Format == DXGI_FORMAT_R32G32B32A32_FLOAT ||
        m_Format == DXGI_FORMAT_R16G16_UINT ||
        m_Format == DXGI_FORMAT_R32G32_UINT)
    {
        m_pGenerateSAT = std::make_unique<GenerateSAT>(m_pShadowTexture.get());
    }
    //Create render targets for texture array
    m_CascadedRenderTargetDescriptors = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, arraySize);
    for (UINT16 i = 0; i < arraySize; ++i)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_Format;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.PlaneSlice = 0;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

        Application::GetApp()->GetDevice()->CreateRenderTargetView(
            m_pShadowTexture->GetD3D12Resource().Get(), &rtvDesc, m_CascadedRenderTargetDescriptors.GetDescriptorHandle(i));
    }
    //Create sub cameras for light camera
    for (int i = 0; i < (int)m_CascadeLevel; ++i)
    {
        m_CascadedCameras[i] = std::make_unique<Camera>(Orthographic);
        m_pLight->GetLightCamera()->SetSubCameras(m_CascadedCameras[i].get(), 1);
    }
    //Compute cascaded factors
    ComputeCascadePartitionFactor();
}

void CascadedVarianceShadow::BeginShadow(std::shared_ptr<CommandList> commandList)
{
    //Before rendering,we need to update buffer firstly
    UpdateShadowInfo();
    //
    m_ShadowPassBuffers.resize(1);
    for (int i = 0; i < (int)m_CascadeLevel; ++i)
    {
        //set constant buffer for each cascade.
        auto LightViewMatrix = m_pLight->GetLightViewMatrix();
        //Use sub cameras of light camera to get proj matrix.
        auto LightProjMatrix = m_CascadedCameras[i]->GetProj();
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].View, DirectX::XMMatrixTranspose(LightViewMatrix));
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].Proj, DirectX::XMMatrixTranspose(LightProjMatrix));
        DirectX::XMStoreFloat4x4(&m_ShadowPassBuffers[0].ViewProj, DirectX::XMMatrixTranspose(LightViewMatrix * LightProjMatrix));

        float clear[4];
        //for standard variance shadow map
        if (m_Format == DXGI_FORMAT_R16G16_FLOAT || m_Format == DXGI_FORMAT_R32G32_FLOAT)
        {
            clear[0] = 1.0f;
            clear[1] = 1.0f;
            clear[2] = 0.0f;
            clear[3] = 0.0f;
        }
        //for variance shadow map fp
        if (m_Format == DXGI_FORMAT_R16G16B16A16_FLOAT || m_Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
        {
            clear[0] = 0.5f;
            clear[1] = 0.5f;
            clear[2] = 0.0f;
            clear[3] = 0.0f;
        }
        //for variance shadow map uint
        if (m_Format == DXGI_FORMAT_R16G16_UINT || m_Format == DXGI_FORMAT_R32G32_UINT)
        {
            clear[0] = 262144.0f;
            clear[1] = 262144.0f;
            clear[2] = 0.0f;
            clear[3] = 0.0f;
        }
        //Clear render target and depth stencil
        {
            commandList->SetD3D12ViewPort(&m_ShadowViewPort);
            commandList->SetD3D12ScissorRect(&m_ShadowScissorRect);
            commandList->ClearRenderTargetTexture(m_pShadowTexture.get(), clear);
            commandList->ClearDepthStencilTexture(m_pShadowDepthTexture.get(), D3D12_CLEAR_FLAG_DEPTH);
            commandList->GetGraphicsCommandList2()->OMSetRenderTargets(1, &m_CascadedRenderTargetDescriptors.GetDescriptorHandle(i),
                FALSE, &m_pShadowDepthTexture->GetDepthStencilView());
        }
        //Here we use sub cameras to do frustum culling.
        m_pShadowFrustumCullinger->BindFrustumCamera(m_CascadedCameras[i].get());
        commandList->RenderShadow(this);
    }

    //for variance shadow map,after rendering depth,we can use box-filter to blur it for soft shadow.
    //Note:only the standard variance shadow map needs to blur.
    if (m_Format == DXGI_FORMAT_R16G16_FLOAT || m_Format == DXGI_FORMAT_R32G32_FLOAT)
    {
        m_pFilter->BeginFilter(commandList);
    }
    //generate sat.
    if (m_Format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
        m_Format == DXGI_FORMAT_R32G32B32A32_FLOAT ||
        m_Format == DXGI_FORMAT_R16G16_UINT ||
        m_Format == DXGI_FORMAT_R32G32_UINT)
    {
        m_pGenerateSAT->GenerateSATs(commandList);
    }
}

void CascadedVarianceShadow::ComputeCascadePartitionFactor()
{
    switch (m_CascadeLevel)
    {
    case CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL1:
        m_CascadePartitionFactor[0] = 1.0f;
        break;
    case CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL2:
        m_CascadePartitionFactor[0] = 0.4f;
        m_CascadePartitionFactor[1] = 1.0f;
        break;
    case CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL3:
        m_CascadePartitionFactor[0] = 0.2f;
        m_CascadePartitionFactor[1] = 0.6f;
        m_CascadePartitionFactor[2] = 1.0f;
        break;
    case CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL4:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.3f;
        m_CascadePartitionFactor[2] = 0.6f;
        m_CascadePartitionFactor[3] = 1.0f;
        break;
    case CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL5:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.25f;
        m_CascadePartitionFactor[2] = 0.4f;
        m_CascadePartitionFactor[3] = 0.6f;
        m_CascadePartitionFactor[4] = 1.0f;
        break;
    case CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL6:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.2f;
        m_CascadePartitionFactor[2] = 0.35f;
        m_CascadePartitionFactor[3] = 0.5f;
        m_CascadePartitionFactor[4] = 0.75f;
        m_CascadePartitionFactor[5] = 1.0f;
        break;
    case CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL7:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.2f;
        m_CascadePartitionFactor[2] = 0.35f;
        m_CascadePartitionFactor[3] = 0.5f;
        m_CascadePartitionFactor[4] = 0.65f;
        m_CascadePartitionFactor[5] = 0.8f;
        m_CascadePartitionFactor[6] = 1.0f;
        break;
    case CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL8:
        m_CascadePartitionFactor[0] = 0.1f;
        m_CascadePartitionFactor[1] = 0.2f;
        m_CascadePartitionFactor[2] = 0.3f;
        m_CascadePartitionFactor[3] = 0.4f;
        m_CascadePartitionFactor[4] = 0.55f;
        m_CascadePartitionFactor[5] = 0.7f;
        m_CascadePartitionFactor[6] = 0.85f;
        m_CascadePartitionFactor[7] = 1.0f;
        break;
    }
}

void CascadedVarianceShadow::UpdateShadowInfo()
{
    assert(m_pMainCamera && "Error!Main camera is null!");
    //Firstly,we need to divide the view space of main camera
    float DepthRange = m_pMainCamera->GetFarZ() - m_pMainCamera->GetNearZ();
    //we loop to compute proj matrix for each cascade
    for (int i = 0; i < (int)m_CascadeLevel; ++i)
    {
        float cascadeDepthStart = 0.0f;
        float cascadeDepthEnd = 0.0f;
        //According to partition mathod to compute slice
        if (m_SelectedCascadedFit == FIT_PROJECTION_TO_CASCADES::FIT_TO_SCENE)
        {
            cascadeDepthStart = 0.0f;
            cascadeDepthEnd = DepthRange * m_CascadePartitionFactor[i];
        }
        else if (m_SelectedCascadedFit == FIT_PROJECTION_TO_CASCADES::FIT_TO_CASCADE)
        {
            if (i == 0) cascadeDepthStart = 0.0f;
            else        cascadeDepthStart = DepthRange * m_CascadePartitionFactor[i - 1];
            cascadeDepthEnd = DepthRange * m_CascadePartitionFactor[i];
        }
        m_CascadePartitionDepth[i] = cascadeDepthEnd;
        //then we use start and end value to compute slice view frustum points
        DirectX::XMVECTOR FrustumCornersViewSpace[8];
        GetFrustumPointsViewSpaceFromInterval(cascadeDepthStart, cascadeDepthEnd, m_pMainCamera->GetProj(), FrustumCornersViewSpace);
        //next,we need to transform these points to light space
        DirectX::XMMATRIX ViewToWorld = m_pMainCamera->GetInvView();
        DirectX::XMMATRIX WorldToLight = m_pLight->GetLightViewMatrix();
        DirectX::XMVECTOR FrustumCornersWorldSpace[8];
        DirectX::XMVECTOR FrustumCornersLightSpace[8];
        DirectX::XMVECTOR FrustumSliceMaxLightSpace = { -FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX };
        DirectX::XMVECTOR FrustumSliceMinLightSpace = { FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX };
        //then,we use these points to compute min and max,and to compute x and y for light frustum.
        for (int i = 0; i < 8; ++i)
        {
            FrustumCornersWorldSpace[i] = DirectX::XMVector3TransformCoord(FrustumCornersViewSpace[i], ViewToWorld);
            FrustumCornersLightSpace[i] = DirectX::XMVector3TransformCoord(FrustumCornersViewSpace[i], ViewToWorld * WorldToLight);
            FrustumSliceMaxLightSpace = DirectX::XMVectorMax(FrustumSliceMaxLightSpace, FrustumCornersLightSpace[i]);
            FrustumSliceMinLightSpace = DirectX::XMVectorMin(FrustumSliceMinLightSpace, FrustumCornersLightSpace[i]);
        }
        //
        DirectX::XMVECTOR WorldUnitsPerTexelVector;
        if (m_SelectedCascadedFit == FIT_PROJECTION_TO_CASCADES::FIT_TO_SCENE)
        {
            DirectX::XMVECTOR Diagonal = DirectX::XMVectorSubtract(
                FrustumCornersWorldSpace[7],
                FrustumCornersWorldSpace[0]);
            Diagonal = DirectX::XMVector3Length(Diagonal);

            XMVECTOR Offset =
                (Diagonal - (FrustumSliceMaxLightSpace - FrustumSliceMinLightSpace)) * gHalfVector;
            Offset *= gVectorZToZero;

            FrustumSliceMaxLightSpace += Offset;
            FrustumSliceMinLightSpace -= Offset;

            float CascadedLength = DirectX::XMVectorGetX(Diagonal);
            float WorldUnitsPerTexel = (CascadedLength / (float)m_Width);
            WorldUnitsPerTexelVector = DirectX::XMVectorSet(WorldUnitsPerTexel, WorldUnitsPerTexel, 0.0f, 0.0f);
        }
        else//fllowing code , I can not understand.......
        {
            // We calculate a looser bound based on the size of the PCF blur.  This ensures us that we're 
            // sampling within the correct map.
            float fScaleDuetoBlureAMT = ((float)(m_FilterSize * 2 + 1)
                / (float)m_Width);
            XMVECTORF32 vScaleDuetoBlureAMT = { fScaleDuetoBlureAMT, fScaleDuetoBlureAMT, 0.0f, 0.0f };

            float fNormalizeByBufferSize = (1.0f / (float)m_Width);
            XMVECTOR vNormalizeByBufferSize = DirectX::XMVectorSet(fNormalizeByBufferSize, fNormalizeByBufferSize, 0.0f, 0.0f);

            // We calculate the offsets as a percentage of the bound.
            XMVECTOR vBoarderOffset = FrustumSliceMaxLightSpace - FrustumSliceMinLightSpace;
            vBoarderOffset *= gHalfVector;
            vBoarderOffset *= vScaleDuetoBlureAMT;
            FrustumSliceMaxLightSpace += vBoarderOffset;
            FrustumSliceMinLightSpace -= vBoarderOffset;

            // The world units per texel are used to snap  the orthographic projection
            // to texel sized increments.  
            // Because we're fitting tighly to the cascades, the shimmering shadow edges will still be present when the 
            // camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
            WorldUnitsPerTexelVector = FrustumSliceMaxLightSpace - FrustumSliceMinLightSpace;
            WorldUnitsPerTexelVector *= vNormalizeByBufferSize;
        }
        //Here we limit camera move footstep into one texel to solve shadows jitter.
        //This is a matter of integer dividing by the world space size of a texel
        FrustumSliceMinLightSpace /= WorldUnitsPerTexelVector;
        FrustumSliceMinLightSpace = XMVectorFloor(FrustumSliceMinLightSpace);
        FrustumSliceMinLightSpace *= WorldUnitsPerTexelVector;

        FrustumSliceMaxLightSpace /= WorldUnitsPerTexelVector;
        FrustumSliceMaxLightSpace = XMVectorFloor(FrustumSliceMaxLightSpace);
        FrustumSliceMaxLightSpace *= WorldUnitsPerTexelVector;
        //Finally,we can use scene AABB and frustum slice to compute znear and zfar of light frustum.
        //Here,we have two method to decide znear and zfar.
        //FIT_TO_SCENE: directly use scene AABB to compute znear and zfar.This method is enough for many cases.
        //But in special case,such as the camera view is small but scene aabb is long,this will lead the light frustum has a large distance between znear and zfar.
        //FIT_TO_CASCADE: use camera frustum slice and scene aabb to compute znear and zfar.This will get a tighter light frustum.
        //for cascaded shadow map,this is a very important technology,since many slice will be smaller than scene aabb.

        //We need to transform the corners of scene AABB to light space
        auto SceneAABBWorldSpace = Scene::GetScene()->GetSceneBoundingBox();
        DirectX::XMFLOAT3 AABBCornersWorldSpace[8];
        SceneAABBWorldSpace.GetCorners(AABBCornersWorldSpace);
        DirectX::XMVECTOR AABBCornerLightSpace[8];
        for (int i = 0; i < 8; ++i)
        {
            AABBCornerLightSpace[i] = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&AABBCornersWorldSpace[i]), m_pLight->GetLightViewMatrix());
        }
        //A simple method.
        //We just compute the min and max of aabb in light space.
        float zNear = 0.0f;
        float zFar = 1000.0f;
        if (m_SelectedCascadedNearFar == FIT_TO_NEAR_FAR::FIT_TO_SCENE_AABB)
        {
            DirectX::XMVECTOR MaxAABBCornersLightSpace = { -FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX };
            DirectX::XMVECTOR MinAABBCornersLightSpace = { FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX };
            //compute min and max
            for (int i = 0; i < 8; ++i)
            {
                MaxAABBCornersLightSpace = DirectX::XMVectorMax(MaxAABBCornersLightSpace, AABBCornerLightSpace[i]);
                MinAABBCornersLightSpace = DirectX::XMVectorMin(MinAABBCornersLightSpace, AABBCornerLightSpace[i]);
            }
            zNear = DirectX::XMVectorGetZ(MinAABBCornersLightSpace);
            zFar = DirectX::XMVectorGetZ(MaxAABBCornersLightSpace);
        }
        //A better method to compute tighter frustum for light.
        else if (m_SelectedCascadedNearFar == FIT_TO_NEAR_FAR::FIT_TO_CASCADE_AABB)
        {
            ComputeNearAndFar(zNear, zFar, FrustumSliceMinLightSpace, FrustumSliceMaxLightSpace, AABBCornerLightSpace);
        }
        //Until now,we get all parameter for light frustum,we can compute light frustum.
        m_CascadedCameras[i]->SetLens(
            DirectX::XMVectorGetX(FrustumSliceMinLightSpace),
            DirectX::XMVectorGetX(FrustumSliceMaxLightSpace),
            DirectX::XMVectorGetY(FrustumSliceMinLightSpace),
            DirectX::XMVectorGetY(FrustumSliceMaxLightSpace),
            zNear,
            zFar);
    }
}

void CascadedVarianceShadow::GetFrustumPointsViewSpaceFromInterval(
    float FrustumClipStart,
    float FrustumClipEnd,
    DirectX::XMMATRIX EyeProjection,
    DirectX::XMVECTOR* FrustumPointsViewSpace)
{
    DirectX::BoundingFrustum FrustumClip;
    DirectX::BoundingFrustum::CreateFromMatrix(FrustumClip, EyeProjection);
    FrustumClip.Near = FrustumClipStart;
    FrustumClip.Far = FrustumClipEnd;

    //Points Layout
    //    Near        Far
    //  0------1    4------5
    //  |      |    |      |  
    //  |      |    |      |
    //  2------3    6------7

    DirectX::XMVECTOR LeftTopSlope = { FrustumClip.LeftSlope,FrustumClip.TopSlope,1.0f,0.0f };
    DirectX::XMVECTOR RightTopSlope = { FrustumClip.RightSlope,FrustumClip.TopSlope,1.0f,0.0f };
    DirectX::XMVECTOR LeftBottomSlope = { FrustumClip.LeftSlope,FrustumClip.BottomSlope,1.0f,0.0f };
    DirectX::XMVECTOR RightBottomSlope = { FrustumClip.RightSlope,FrustumClip.BottomSlope,1.0f,0.0f };

    DirectX::XMVECTOR Near = DirectX::XMVectorReplicate(FrustumClip.Near);
    DirectX::XMVECTOR Far = DirectX::XMVectorReplicate(FrustumClip.Far);

    DirectX::XMVECTOR Points[8];
    //Near
    Points[0] = DirectX::XMVectorMultiply(LeftTopSlope, Near);
    Points[1] = DirectX::XMVectorMultiply(RightTopSlope, Near);
    Points[2] = DirectX::XMVectorMultiply(LeftBottomSlope, Near);
    Points[3] = DirectX::XMVectorMultiply(RightBottomSlope, Near);
    //Far
    Points[4] = DirectX::XMVectorMultiply(LeftTopSlope, Far);
    Points[5] = DirectX::XMVectorMultiply(RightTopSlope, Far);
    Points[6] = DirectX::XMVectorMultiply(LeftBottomSlope, Far);
    Points[7] = DirectX::XMVectorMultiply(RightBottomSlope, Far);

    for (int i = 0; i < 8; ++i)
    {
        FrustumPointsViewSpace[i] = DirectX::XMVectorAdd(
            DirectX::XMVector3Rotate(Points[i], DirectX::XMLoadFloat4(&FrustumClip.Orientation)),
            DirectX::XMLoadFloat3(&FrustumClip.Origin));
    }
}

void CascadedVarianceShadow::ComputeNearAndFar(
    FLOAT& fNearPlane,
    FLOAT& fFarPlane,
    DirectX::FXMVECTOR vLightCameraOrthographicMin,
    DirectX::FXMVECTOR vLightCameraOrthographicMax,
    DirectX::XMVECTOR* pvPointsInCameraView)
{
    // Initialize the near and far planes
    fNearPlane = FLT_MAX;
    fFarPlane = -FLT_MAX;

    Triangle triangleList[16];
    INT iTriangleCnt = 1;

    triangleList[0].pt[0] = pvPointsInCameraView[0];
    triangleList[0].pt[1] = pvPointsInCameraView[1];
    triangleList[0].pt[2] = pvPointsInCameraView[2];
    triangleList[0].culled = false;

    // These are the indices used to tesselate an AABB into a list of triangles.
    static const INT iAABBTriIndexes[] =
    {
        0,1,2,  0,2,3,
        4,5,6,  4,6,7,
        0,4,7,  0,3,7,
        1,6,5,  1,2,6,
        0,1,4,  1,4,5,
        2,3,6,  3,6,7
    };

    INT iPointPassesCollision[3];

    // At a high level: 
    // 1. Iterate over all 12 triangles of the AABB.  
    // 2. Clip the triangles against each plane. Create new triangles as needed.
    // 3. Find the min and max z values as the near and far plane.

    //This is easier because the triangles are in camera spacing making the collisions tests simple comparisions.

    float fLightCameraOrthographicMinX = DirectX::XMVectorGetX(vLightCameraOrthographicMin);
    float fLightCameraOrthographicMaxX = DirectX::XMVectorGetX(vLightCameraOrthographicMax);
    float fLightCameraOrthographicMinY = DirectX::XMVectorGetY(vLightCameraOrthographicMin);
    float fLightCameraOrthographicMaxY = DirectX::XMVectorGetY(vLightCameraOrthographicMax);

    for (INT AABBTriIter = 0; AABBTriIter < 12; ++AABBTriIter)
    {

        triangleList[0].pt[0] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 0]];
        triangleList[0].pt[1] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 1]];
        triangleList[0].pt[2] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 2]];
        iTriangleCnt = 1;
        triangleList[0].culled = FALSE;

        // Clip each invidual triangle against the 4 frustums.  When ever a triangle is clipped into new triangles, 
        //add them to the list.
        for (INT frustumPlaneIter = 0; frustumPlaneIter < 4; ++frustumPlaneIter)
        {

            FLOAT fEdge;
            INT iComponent;

            if (frustumPlaneIter == 0)
            {
                fEdge = fLightCameraOrthographicMinX; // todo make float temp
                iComponent = 0;
            }
            else if (frustumPlaneIter == 1)
            {
                fEdge = fLightCameraOrthographicMaxX;
                iComponent = 0;
            }
            else if (frustumPlaneIter == 2)
            {
                fEdge = fLightCameraOrthographicMinY;
                iComponent = 1;
            }
            else
            {
                fEdge = fLightCameraOrthographicMaxY;
                iComponent = 1;
            }

            for (INT triIter = 0; triIter < iTriangleCnt; ++triIter)
            {
                // We don't delete triangles, so we skip those that have been culled.
                if (!triangleList[triIter].culled)
                {
                    INT iInsideVertCount = 0;
                    DirectX::XMVECTOR tempOrder;
                    // Test against the correct frustum plane.
                    // This could be written more compactly, but it would be harder to understand.

                    if (frustumPlaneIter == 0)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (DirectX::XMVectorGetX(triangleList[triIter].pt[triPtIter]) >
                                DirectX::XMVectorGetX(vLightCameraOrthographicMin))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if (frustumPlaneIter == 1)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (DirectX::XMVectorGetX(triangleList[triIter].pt[triPtIter]) <
                                DirectX::XMVectorGetX(vLightCameraOrthographicMax))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if (frustumPlaneIter == 2)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (DirectX::XMVectorGetY(triangleList[triIter].pt[triPtIter]) >
                                DirectX::XMVectorGetY(vLightCameraOrthographicMin))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (DirectX::XMVectorGetY(triangleList[triIter].pt[triPtIter]) <
                                DirectX::XMVectorGetY(vLightCameraOrthographicMax))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }

                    // Move the points that pass the frustum test to the begining of the array.
                    if (iPointPassesCollision[1] && !iPointPassesCollision[0])
                    {
                        tempOrder = triangleList[triIter].pt[0];
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;
                        iPointPassesCollision[1] = FALSE;
                    }
                    if (iPointPassesCollision[2] && !iPointPassesCollision[1])
                    {
                        tempOrder = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter].pt[2];
                        triangleList[triIter].pt[2] = tempOrder;
                        iPointPassesCollision[1] = TRUE;
                        iPointPassesCollision[2] = FALSE;
                    }
                    if (iPointPassesCollision[1] && !iPointPassesCollision[0])
                    {
                        tempOrder = triangleList[triIter].pt[0];
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;
                        iPointPassesCollision[1] = FALSE;
                    }

                    if (iInsideVertCount == 0)
                    { // All points failed. We're done,  
                        triangleList[triIter].culled = true;
                    }
                    else if (iInsideVertCount == 1)
                    {// One point passed. Clip the triangle against the Frustum plane
                        triangleList[triIter].culled = false;

                        // 
                        DirectX::XMVECTOR vVert0ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[0];
                        DirectX::XMVECTOR vVert0ToVert2 = triangleList[triIter].pt[2] - triangleList[triIter].pt[0];

                        // Find the collision ratio.
                        FLOAT fHitPointTimeRatio = fEdge - DirectX::XMVectorGetByIndex(triangleList[triIter].pt[0], iComponent);
                        // Calculate the distance along the vector as ratio of the hit ratio to the component.
                        FLOAT fDistanceAlongVector01 = fHitPointTimeRatio / DirectX::XMVectorGetByIndex(vVert0ToVert1, iComponent);
                        FLOAT fDistanceAlongVector02 = fHitPointTimeRatio / DirectX::XMVectorGetByIndex(vVert0ToVert2, iComponent);
                        // Add the point plus a percentage of the vector.
                        vVert0ToVert1 *= fDistanceAlongVector01;
                        vVert0ToVert1 += triangleList[triIter].pt[0];
                        vVert0ToVert2 *= fDistanceAlongVector02;
                        vVert0ToVert2 += triangleList[triIter].pt[0];

                        triangleList[triIter].pt[1] = vVert0ToVert2;
                        triangleList[triIter].pt[2] = vVert0ToVert1;

                    }
                    else if (iInsideVertCount == 2)
                    { // 2 in  // tesselate into 2 triangles


                        // Copy the triangle\(if it exists) after the current triangle out of
                        // the way so we can override it with the new triangle we're inserting.
                        triangleList[iTriangleCnt] = triangleList[triIter + 1];

                        triangleList[triIter].culled = false;
                        triangleList[triIter + 1].culled = false;

                        // Get the vector from the outside point into the 2 inside points.
                        XMVECTOR vVert2ToVert0 = triangleList[triIter].pt[0] - triangleList[triIter].pt[2];
                        XMVECTOR vVert2ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[2];

                        // Get the hit point ratio.
                        FLOAT fHitPointTime_2_0 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
                        FLOAT fDistanceAlongVector_2_0 = fHitPointTime_2_0 / XMVectorGetByIndex(vVert2ToVert0, iComponent);
                        // Calcaulte the new vert by adding the percentage of the vector plus point 2.
                        vVert2ToVert0 *= fDistanceAlongVector_2_0;
                        vVert2ToVert0 += triangleList[triIter].pt[2];

                        // Add a new triangle.
                        triangleList[triIter + 1].pt[0] = triangleList[triIter].pt[0];
                        triangleList[triIter + 1].pt[1] = triangleList[triIter].pt[1];
                        triangleList[triIter + 1].pt[2] = vVert2ToVert0;

                        //Get the hit point ratio.
                        FLOAT fHitPointTime_2_1 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
                        FLOAT fDistanceAlongVector_2_1 = fHitPointTime_2_1 / XMVectorGetByIndex(vVert2ToVert1, iComponent);
                        vVert2ToVert1 *= fDistanceAlongVector_2_1;
                        vVert2ToVert1 += triangleList[triIter].pt[2];
                        triangleList[triIter].pt[0] = triangleList[triIter + 1].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter + 1].pt[2];
                        triangleList[triIter].pt[2] = vVert2ToVert1;
                        // Cncrement triangle count and skip the triangle we just inserted.
                        ++iTriangleCnt;
                        ++triIter;


                    }
                    else
                    { // all in
                        triangleList[triIter].culled = false;

                    }
                }// end if !culled loop            
            }
        }
        for (INT index = 0; index < iTriangleCnt; ++index)
        {
            if (!triangleList[index].culled)
            {
                // Set the near and far plan and the min and max z values respectivly.
                for (int vertind = 0; vertind < 3; ++vertind)
                {
                    float fTriangleCoordZ = XMVectorGetZ(triangleList[index].pt[vertind]);
                    if (fNearPlane > fTriangleCoordZ)
                    {
                        fNearPlane = fTriangleCoordZ;
                    }
                    if (fFarPlane < fTriangleCoordZ)
                    {
                        fFarPlane = fTriangleCoordZ;
                    }
                }
            }
        }
    }
}
