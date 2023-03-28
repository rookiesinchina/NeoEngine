#include "Filter.h"
#include "Texture.h"
#include "RootSignature.h"
#include "Application.h"
#include "CommandList.h"

#include <DirectXColors.h>

Filter::Filter(const Texture* pTexture, FILTER_RADIUS Radius,FILTER_TYPE Type)
    : m_Radius(Radius)
    , m_pTexture(pTexture)
    , m_FilterType(Type)
{
    assert(m_pTexture && "Error!Input Texture is null");
    auto device = Application::GetApp()->GetDevice();
    auto TexDesc = m_pTexture->GetD3D12ResourceDesc();
    if (TexDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D && TexDesc.MipLevels != 1 && TexDesc.SampleDesc.Count > 1)
    {
        assert(FALSE && "Error!The filter instance can only filter 2D non-ms texture with mip 1");
        return;
    }
    //Create ping pong textures and views
    auto pingpongDesc = TexDesc;
    pingpongDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    pingpongDesc.Width = (UINT64)ceil(CalculateOutputTextureScalingFactor() * TexDesc.Width);
    pingpongDesc.Height = (UINT64)ceil(CalculateOutputTextureScalingFactor() * TexDesc.Height);

    D3D12_CLEAR_VALUE Clear = CD3DX12_CLEAR_VALUE(pingpongDesc.Format, DirectX::Colors::White);
    m_pPingPongTexture0 = std::make_unique<Texture>(&pingpongDesc, &Clear, TextureUsage::RenderTargetTexture, L"Filter RenderTarget0");
    m_pPingPongTexture1 = std::make_unique<Texture>(&pingpongDesc, &Clear, TextureUsage::RenderTargetTexture, L"Filter RenderTarget1");
    //Create result texture
    auto resultDesc = TexDesc;
    resultDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    m_pResultTexture = std::make_unique<Texture>(&resultDesc, &Clear, TextureUsage::RenderTargetTexture, L"Filter Result Texture");
    //
    m_ViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, pingpongDesc.Width, pingpongDesc.Height);
    m_ScissorRect = { 0,0,(int)pingpongDesc.Width,(int)pingpongDesc.Height };
    //If input texture is a texture array,we need to create rtv and srv.
    //Note:here we see the texture2D as a special case of texture array which has only one slice.
    D3D12_RENDER_TARGET_VIEW_DESC RtvDesc = {};
    RtvDesc.Format = TexDesc.Format;
    RtvDesc.Texture2DArray.ArraySize = TexDesc.DepthOrArraySize;
    RtvDesc.Texture2DArray.FirstArraySlice = 0;
    RtvDesc.Texture2DArray.MipSlice = 0;
    RtvDesc.Texture2DArray.PlaneSlice = 0;
    RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    m_RtvDescriptors = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3);
    device->CreateRenderTargetView(m_pPingPongTexture0->GetD3D12Resource().Get(), &RtvDesc, m_RtvDescriptors.GetDescriptorHandle(0));
    device->CreateRenderTargetView(m_pPingPongTexture1->GetD3D12Resource().Get(), &RtvDesc, m_RtvDescriptors.GetDescriptorHandle(1));
    device->CreateRenderTargetView(m_pResultTexture->GetD3D12Resource().Get(), &RtvDesc, m_RtvDescriptors.GetDescriptorHandle(2));
    //Create filter constant
    switch (m_FilterType)
    {
    case FILTER_TYPE::FILTER_BOX:
        m_FilterConstant.Weights = { 0.2f,0.4f };
        m_FilterConstant.Offsets = { 0.0f,1.5f };
        m_FilterConstant.InputTextureSize = { (float)pingpongDesc.Width,
                                              (float)pingpongDesc.Height };
        m_FilterConstant.TextureArraySize = TexDesc.DepthOrArraySize;
        break;
    case FILTER_TYPE::FILTER_GAUSSIAN:
        m_FilterConstant.Weights = { 0.4026f,0.2987f };
        m_FilterConstant.Offsets = { 0.0f,1.182457f };
        m_FilterConstant.InputTextureSize = { (float)pingpongDesc.Width,
                                              (float)pingpongDesc.Height };
        m_FilterConstant.TextureArraySize = TexDesc.DepthOrArraySize;
        break;
    }
    //Create root signature
    D3D12_FEATURE_DATA_ROOT_SIGNATURE rootVersion = {};
    rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(Application::GetApp()->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootVersion, sizeof(rootVersion))))
    {
        rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    CD3DX12_DESCRIPTOR_RANGE1 range = {};
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_ROOT_PARAMETER1 rootParameters[FilterRootParameters::NumFilterRootParameters];
    rootParameters[FilterRootParameters::FilterConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[FilterRootParameters::FilterTexture].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_ROOT_SIGNATURE_FLAGS Flags = 
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS | 
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;

    CD3DX12_STATIC_SAMPLER_DESC linearClamp = {};
    linearClamp.Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc = {};
    versionRootSignatureDesc.Init_1_1(FilterRootParameters::NumFilterRootParameters, rootParameters, 1, &linearClamp, Flags);

    m_pRootSignature = std::make_unique<RootSignature>(versionRootSignatureDesc.Desc_1_1, rootVersion.HighestVersion);
    //create pipeline state
    struct PipelineState
    {
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendDesc;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL         DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
        CD3DX12_PIPELINE_STATE_STREAM_GS                    GS;
        CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargetFormats;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DepthStencilFormat;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
    }pipelinestate;
    D3D_SHADER_MACRO Sample[] =
    {
        "DOWN_OR_UP_SAMPLING","1",
        NULL,NULL
    };
    D3D_SHADER_MACRO FilterH[] =
    {
        "FILTER_HORIZONTAL","1",
        NULL,NULL
    };
    D3D_SHADER_MACRO FilterV[] =
    {
        "FILTER_VERTICAL","1",
        NULL,NULL
    };
    
    Microsoft::WRL::ComPtr<ID3DBlob> vs = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Filter.hlsl", nullptr, "VS", "vs_5_1");
    Microsoft::WRL::ComPtr<ID3DBlob> gs = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Filter.hlsl", nullptr, "GS", "gs_5_1");
    Microsoft::WRL::ComPtr<ID3DBlob> ps_sample = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Filter.hlsl", Sample, "PS", "ps_5_1");
    Microsoft::WRL::ComPtr<ID3DBlob> ps_filterH = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Filter.hlsl", FilterH, "PS", "ps_5_1");
    Microsoft::WRL::ComPtr<ID3DBlob> ps_filterV = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Filter.hlsl", FilterV, "PS", "ps_5_1");
    D3D12_RT_FORMAT_ARRAY RtArray = {};
    RtArray.NumRenderTargets = 1;
    RtArray.RTFormats[0] = m_pTexture->GetD3D12ResourceDesc().Format;

    pipelinestate.BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipelinestate.DepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipelinestate.DepthStencilFormat = DXGI_FORMAT_UNKNOWN;
    pipelinestate.InputLayout = { nullptr,0 };
    pipelinestate.PrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelinestate.pRootSignature = m_pRootSignature->GetRootSignature().Get();
    pipelinestate.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
    pipelinestate.GS = CD3DX12_SHADER_BYTECODE(gs.Get());
    pipelinestate.PS = CD3DX12_SHADER_BYTECODE(ps_sample.Get());
    pipelinestate.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipelinestate.RenderTargetFormats = RtArray;
    pipelinestate.SampleDesc = { 1,0 };
    
    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = { sizeof(pipelinestate) ,&pipelinestate, };
    ThrowIfFailed(Application::GetApp()->GetDevice()->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_d3d12SamplePipelineState)));

    pipelinestate.PS = CD3DX12_SHADER_BYTECODE(ps_filterH.Get());
    streamDesc = { sizeof(pipelinestate) ,&pipelinestate, };
    ThrowIfFailed(Application::GetApp()->GetDevice()->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_d3d12FilterHPipelineState)));

    pipelinestate.PS = CD3DX12_SHADER_BYTECODE(ps_filterV.Get());
    streamDesc = { sizeof(pipelinestate) ,&pipelinestate, };
    ThrowIfFailed(Application::GetApp()->GetDevice()->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_d3d12FilterVPipelineState)));
}

void Filter::BeginFilter(std::shared_ptr<CommandList> commandList)
{
    //if the input texture is a texture array,we need to create srv for them
    //Note:here we see the texture2D as a special case of texture array which has only one slice.
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Format = m_pTexture->GetD3D12ResourceDesc().Format;
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Texture2DArray.ArraySize = m_pTexture->GetD3D12ResourceDesc().DepthOrArraySize;
    SrvDesc.Texture2DArray.FirstArraySlice = 0;
    SrvDesc.Texture2DArray.MipLevels = 1;
    SrvDesc.Texture2DArray.MostDetailedMip = 0;
    SrvDesc.Texture2DArray.PlaneSlice = 0;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    commandList->SetD3D12ViewPort(&m_ViewPort);
    commandList->SetD3D12ScissorRect(&m_ScissorRect);

    commandList->SetGraphicsRootSignature(m_pRootSignature.get());
    //if the blur size is too large,we need to downsample origin texture firstly
    //we render it to ping-pong1
    if ((int)m_Radius > (int)FILTER_RADIUS::FILTER_RADIUS_2)
    {
        commandList->SetD3D12PipelineState(m_d3d12SamplePipelineState);
        commandList->ClearRenderTargetTexture(m_pPingPongTexture1.get(), DirectX::Colors::White);
        commandList->GetGraphicsCommandList2()->OMSetRenderTargets(1, &m_RtvDescriptors.GetDescriptorHandle(1), FALSE, nullptr);
        commandList->SetGraphicsDynamicConstantBuffer(FilterRootParameters::FilterConstantBuffer, m_FilterConstant);
        commandList->SetShaderResourceView(FilterRootParameters::FilterTexture, 0, m_pTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
        commandList->Draw(6, 1, 0, 0);
    }
    //Firstly,we use input texture to sample along horizontal direction and write to ping-pong0 texture
    commandList->SetD3D12PipelineState(m_d3d12FilterHPipelineState);
    commandList->ClearRenderTargetTexture(m_pPingPongTexture0.get(), DirectX::Colors::White);
    commandList->GetGraphicsCommandList2()->OMSetRenderTargets(1, &m_RtvDescriptors.GetDescriptorHandle(0), FALSE, nullptr);
    commandList->SetGraphicsDynamicConstantBuffer(FilterRootParameters::FilterConstantBuffer, m_FilterConstant);
    commandList->SetShaderResourceView(
        FilterRootParameters::FilterTexture, 0,
        (int)m_Radius > (int)FILTER_RADIUS::FILTER_RADIUS_2 ? m_pPingPongTexture1.get() : m_pTexture,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
    commandList->Draw(6, 1, 0, 0);
    //Secondly,we use ping-pong1 as srv to sample along vertical direction and write to ping-pong1 texture
    commandList->SetD3D12PipelineState(m_d3d12FilterVPipelineState);
    commandList->ClearRenderTargetTexture(m_pPingPongTexture1.get(), DirectX::Colors::White,true);
    commandList->GetGraphicsCommandList2()->OMSetRenderTargets(1, &m_RtvDescriptors.GetDescriptorHandle(1), FALSE, nullptr);
    commandList->SetGraphicsDynamicConstantBuffer(FilterRootParameters::FilterConstantBuffer, m_FilterConstant);
    commandList->SetShaderResourceView(FilterRootParameters::FilterTexture, 0, m_pPingPongTexture0.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
    commandList->Draw(6, 1, 0, 0);
    //Finally,if we downsample input texture,we need to upsample to original texture size.
    if ((int)m_Radius > (int)FILTER_RADIUS::FILTER_RADIUS_2)
    {
        D3D12_VIEWPORT OriginViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, m_pTexture->GetD3D12ResourceDesc().Width, m_pTexture->GetD3D12ResourceDesc().Height);
        RECT OriginRect = { 0,0,(int)m_pTexture->GetD3D12ResourceDesc().Width,(int)m_pTexture->GetD3D12ResourceDesc().Height };
        commandList->SetD3D12ViewPort(&OriginViewPort);
        commandList->SetD3D12ScissorRect(&OriginRect);
        commandList->SetD3D12PipelineState(m_d3d12SamplePipelineState);
        commandList->ClearRenderTargetTexture(m_pResultTexture.get(), DirectX::Colors::White);
        commandList->GetGraphicsCommandList2()->OMSetRenderTargets(1, &m_RtvDescriptors.GetDescriptorHandle(2), FALSE, nullptr);
        commandList->SetGraphicsDynamicConstantBuffer(FilterRootParameters::FilterConstantBuffer, m_FilterConstant);
        commandList->SetShaderResourceView(FilterRootParameters::FilterTexture, 0, m_pPingPongTexture1.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
        commandList->Draw(6, 1, 0, 0);
    }
}

const Texture* Filter::GetFilterTexture()const
{
    return (int)m_Radius > (int)FILTER_RADIUS::FILTER_RADIUS_2 ? m_pResultTexture.get() : m_pPingPongTexture1.get();
}

void Filter::SetFilterRadius(FILTER_RADIUS Radius)
{
    if (Radius != m_Radius)
    {
        m_Radius = Radius;
        Resize();
    }
}

float Filter::CalculateOutputTextureScalingFactor()
{
    int textureScaling = (int)m_Radius / (int)FILTER_RADIUS::FILTER_RADIUS_2;
    float scaling = 1.0f / (float)textureScaling;
    return scaling;
}

void Filter::Resize()
{
    int newWidth = ceil(m_pTexture->GetD3D12ResourceDesc().Width * CalculateOutputTextureScalingFactor());
    int newHeight = ceil(m_pTexture->GetD3D12ResourceDesc().Height * CalculateOutputTextureScalingFactor());
    m_pPingPongTexture0->Resize(newWidth, newHeight);
    m_pPingPongTexture1->Resize(newWidth, newHeight);
    m_ViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, m_pPingPongTexture0->GetD3D12ResourceDesc().Width, m_pPingPongTexture0->GetD3D12ResourceDesc().Height);
    m_ScissorRect = { 0,0,(int)m_pPingPongTexture0->GetD3D12ResourceDesc().Width,(int)m_pPingPongTexture0->GetD3D12ResourceDesc().Height };
}