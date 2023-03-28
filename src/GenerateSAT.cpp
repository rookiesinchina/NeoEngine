#include "GenerateSAT.h"
#include "Application.h"
#include "RootSignature.h"
#include "d3dUtil.h"
#include "Texture.h"
#include "CommandQueue.h"
#include "CommandList.h"

GenerateSAT::GenerateSAT(const Texture* pInputTexture)
    :m_pTexture(pInputTexture)
{
    assert(pInputTexture && "Error!Input texture is null");
    auto textureDesc = pInputTexture->GetD3D12ResourceDesc();
    if (textureDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || textureDesc.MipLevels != 1 || textureDesc.SampleDesc.Count > 1)
    {
        assert(FALSE && "Error!We can only generate SAT for non-ms 2D texture with one mip");
        return;
    }
    //Check format
    if (!Texture::IsUavCompatibleFormat(textureDesc.Format))
    {
        assert(FALSE && "Error!This format is not supported by UAV");
    }
    //Check size
    if (textureDesc.Width != textureDesc.Height)
    {
        assert(FALSE && "Error!Since the shader,we can not generate SAT for non-square texture");
    }
    auto device = Application::GetApp()->GetDevice();
    //Create ping-pong textures for sat
    auto pingpongDesc = textureDesc;
    pingpongDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    m_pPingPong0Texture = std::make_unique<Texture>(&pingpongDesc, nullptr, TextureUsage::Diffuse, L"Ping-Pong Texture0 SAT");
    m_pPingPong1Texture = std::make_unique<Texture>(&pingpongDesc, nullptr, TextureUsage::Diffuse, L"Ping-Pong Texture1 SAT");
    //create root signature for sat
    D3D12_FEATURE_DATA_ROOT_SIGNATURE rootVersion = {};
    rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootVersion, sizeof(rootVersion))))
    {
        rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    CD3DX12_DESCRIPTOR_RANGE1 range[2] = {};
    range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0);
    range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 1);
    CD3DX12_ROOT_PARAMETER1 rootParameter;
    rootParameter.InitAsDescriptorTable(2, range, D3D12_SHADER_VISIBILITY_ALL);
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSigDesc = {};
    D3D12_ROOT_SIGNATURE_FLAGS Flags = 
        D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
    versionRootSigDesc.Init_1_1(1, &rootParameter, 0, nullptr, Flags);
    m_pRootSignature = std::make_unique<RootSignature>(versionRootSigDesc.Desc_1_1, rootVersion.HighestVersion);
    //create pipeline state
    struct SATpipelineState
    {
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
    };

    Microsoft::WRL::ComPtr<ID3DBlob> cs_sum;
    Microsoft::WRL::ComPtr<ID3DBlob> cs_sat;
    if (textureDesc.Format == DXGI_FORMAT_R16G16_UINT || textureDesc.Format == DXGI_FORMAT_R32G32_UINT)
    {
        D3D_SHADER_MACRO macro[] =
        {
            "SAT_UINT","1",
            NULL,NULL
        };
        cs_sum = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\GenerateSAT.hlsl", macro, "SumInGroup", "cs_5_1");
        cs_sat = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\GenerateSAT.hlsl", macro, "GenerateSAT", "cs_5_1");
    }
    else
    {
        cs_sum = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\GenerateSAT.hlsl", nullptr, "SumInGroup", "cs_5_1");
        cs_sat = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\GenerateSAT.hlsl", nullptr, "GenerateSAT", "cs_5_1");
    }
    SATpipelineState SumInGroupPipelineState;
    SumInGroupPipelineState.CS = CD3DX12_SHADER_BYTECODE(cs_sum.Get());
    SumInGroupPipelineState.pRootSignature = m_pRootSignature->GetRootSignature().Get();

    SATpipelineState SATPipelineState;
    SATPipelineState.CS = CD3DX12_SHADER_BYTECODE(cs_sat.Get());
    SATPipelineState.pRootSignature = m_pRootSignature->GetRootSignature().Get();

    D3D12_PIPELINE_STATE_STREAM_DESC sumStreamDesc = { sizeof(SumInGroupPipelineState),&SumInGroupPipelineState };
    D3D12_PIPELINE_STATE_STREAM_DESC satStreamDesc = { sizeof(SATPipelineState),&SATPipelineState };

    ThrowIfFailed(device->CreatePipelineState(&sumStreamDesc, IID_PPV_ARGS(&m_d3d12SumInGroupPipelineState)));
    ThrowIfFailed(device->CreatePipelineState(&satStreamDesc, IID_PPV_ARGS(&m_d3d12SATPipelineState)));
}

void GenerateSAT::GenerateSATs(std::shared_ptr<CommandList> commandList)
{
    auto ComputeCommandList = commandList;
    //If the current command list is not a compute command list.we create a new one
    if (commandList->GetCommandListType() == D3D12_COMMAND_LIST_TYPE_COPY)
    {
        auto commandQueue = Application::GetApp()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        ComputeCommandList = commandQueue->GetCommandList();
    }
    auto textureDesc = m_pTexture->GetD3D12ResourceDesc();
    //If the input texture is a texture array,we need to create a srv and uav
    //Note:for compatibility,we use texturue2d as a texture array which has arraysize is one.
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC UavDesc = {};
    SrvDesc.Format = textureDesc.Format;
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Texture2DArray.ArraySize = textureDesc.DepthOrArraySize;
    SrvDesc.Texture2DArray.FirstArraySlice = 0;
    SrvDesc.Texture2DArray.MipLevels = 1;
    SrvDesc.Texture2DArray.MostDetailedMip = 0;
    SrvDesc.Texture2DArray.PlaneSlice = 0;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

    UavDesc.Format = textureDesc.Format;
    UavDesc.Texture2DArray.ArraySize = textureDesc.DepthOrArraySize;
    UavDesc.Texture2DArray.FirstArraySlice = 0;
    UavDesc.Texture2DArray.MipSlice = 0;
    UavDesc.Texture2DArray.PlaneSlice = 0;
    UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    //Start to generate sat
    {
        ComputeCommandList->SetD3D12PipelineState(m_d3d12SumInGroupPipelineState);
        ComputeCommandList->SetComputeRootSignature(m_pRootSignature.get());
        //Firstly,we use input texture to sum in thread group and write to ping-pong0 texture.
        ComputeCommandList->SetShaderResourceView(0, 0, m_pTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
        ComputeCommandList->SetUnorderedAccessView(0, 1, m_pPingPong0Texture.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &UavDesc);
        ComputeCommandList->Dispatch((int)textureDesc.Width / 256, (int)textureDesc.Height, textureDesc.DepthOrArraySize);
        //Secondly,we use ping-pong0 texture to compute SAT in horizontal direction and write to ping-pong1 texture.
        ComputeCommandList->SetD3D12PipelineState(m_d3d12SATPipelineState);
        ComputeCommandList->SetShaderResourceView(0, 0, m_pPingPong0Texture.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
        ComputeCommandList->SetUnorderedAccessView(0, 1, m_pPingPong1Texture.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &UavDesc);
        ComputeCommandList->Dispatch((int)textureDesc.Width / 256, (int)textureDesc.Height, textureDesc.DepthOrArraySize);
        //Thirdly,we use ping-pong1 texture to sum in vertical thread group and write to ping-pong0 texture.
        //Note:since we transpose texture during first pass,so we still dispatch in X.For more details,see GenerateSAT shader.
        ComputeCommandList->SetD3D12PipelineState(m_d3d12SumInGroupPipelineState);
        ComputeCommandList->SetShaderResourceView(0, 0, m_pPingPong1Texture.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
        ComputeCommandList->SetUnorderedAccessView(0, 1, m_pPingPong0Texture.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &UavDesc);
        ComputeCommandList->Dispatch((int)textureDesc.Width / 256, (int)textureDesc.Height, textureDesc.DepthOrArraySize);
        //Finally,we use ping-pong0 texture to compute finally SAT and write to ping-pong1 texture
        ComputeCommandList->SetD3D12PipelineState(m_d3d12SATPipelineState);
        ComputeCommandList->SetShaderResourceView(0, 0, m_pPingPong0Texture.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
        ComputeCommandList->SetUnorderedAccessView(0, 1, m_pPingPong1Texture.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &UavDesc);
        ComputeCommandList->Dispatch((int)textureDesc.Width / 256, (int)textureDesc.Height, textureDesc.DepthOrArraySize);
    }
    //If we use a new compute command queue,we need to wait these commands finish
    if (ComputeCommandList != commandList)
    {
        auto commandQueue = Application::GetApp()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        auto fence = commandQueue->ExecuteCommandList(ComputeCommandList);
        commandQueue->WaitForFenceValue(fence);
    }
}