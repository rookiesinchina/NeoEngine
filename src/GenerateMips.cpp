#include "GenerateMips.h"
#include "Application.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "RootSignature.h"
#include <d3dx12.h>

GenerateMips::GenerateMips()
{
    auto device = Application::GetApp()->GetDevice();
    auto commandQueue = Application::GetApp()->GetCommandQueue();
    auto commandList = commandQueue->GetCommandList();
    m_ComputeRootSignature = std::make_unique<RootSignature>();
    //Allocate descriptors for UAVs
    m_DefaultUAV = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);
        //Since these resources are created during runtime,we need to create UAVs here
    for (UINT i = 0; i < 4; ++i)
    {
        //here format is not important.
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Texture2D.MipSlice = i;
        uavDesc.Texture2D.PlaneSlice = 0;
        uavDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_DefaultUAV.GetDescriptorHandle(i));
    }
    //Check root signature highest version
    D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSigVersion = {};
    rootSigVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSigVersion, sizeof(rootSigVersion))))
    {
        rootSigVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
        //Initialize descriptor range.
    CD3DX12_DESCRIPTOR_RANGE1 SrcDescriptorRange = {};
    SrcDescriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    CD3DX12_DESCRIPTOR_RANGE1 UavDescriptorRange = {};
    UavDescriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
        //Initialize root parameter
    CD3DX12_ROOT_PARAMETER1 rootParameters[GenerateMipsRoot::RootParameter::NumRootParameters];
    rootParameters[GenerateMipsRoot::RootParameter::GenerateMipsCB].InitAsConstants(sizeof(GenerateMipsCB) / sizeof(uint32_t),0);
    rootParameters[GenerateMipsRoot::RootParameter::Src].InitAsDescriptorTable(1, &SrcDescriptorRange);
    rootParameters[GenerateMipsRoot::RootParameter::Mips].InitAsDescriptorTable(1, &UavDescriptorRange);
        //Initialize static samplers
    CD3DX12_STATIC_SAMPLER_DESC LinearClampSamplers = {};
    LinearClampSamplers.Init(
        0, 
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, 
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
        //Set flags for optimation
    D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
                                       D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                       D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                                       D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                       D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                                       D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
        //Initalize versioned root signature desc
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSigDesc = {};
    versionedRootSigDesc.Init_1_1(
        GenerateMipsRoot::RootParameter::NumRootParameters, 
        rootParameters,
        1, 
        &LinearClampSamplers,
        Flags);
        //Set root signauture
    m_ComputeRootSignature->SetRootSignatureDesc(versionedRootSigDesc.Desc_1_1, rootSigVersion.HighestVersion);
    //Load Compute Shaders
    Microsoft::WRL::ComPtr<ID3DBlob> GenerateMipsShader = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\GenerateMips.hlsl", nullptr, "CS", "cs_5_1");
        //Set compute pipeline state
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS             CS;
    }pipelineStateStream;
    pipelineStateStream.pRootSignature = m_ComputeRootSignature->GetRootSignature().Get();
    pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(GenerateMipsShader.Get());

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        sizeof(pipelineStateStream),&pipelineStateStream
    };
        //Create pipeline state 
    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_d3d12ComputePipelineState)));
}

GenerateMips::~GenerateMips() {};

