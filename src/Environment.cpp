#include "Environment.h"
#include "d3dx12.h"
#include "Application.h"
#include "RootSignature.h"
#include "d3dUtil.h"
#include "Mesh.h"
#include "CommandList.h"

std::unordered_map<std::wstring, std::shared_ptr<Texture>> Environment::m_EnvironmentMaps;
std::vector<std::shared_ptr<Texture>> Environment::m_EnvironmentArray;
Environment* Environment::ms_pEnvironmnet = nullptr;

Environment::Environment(std::shared_ptr<CommandList> commandList)
    :m_pRootSignature(std::make_unique<RootSignature>())
    , m_SkyMesh(Mesh::CreateSphere(*commandList))
    , m_CurrentEnvironmentIndex(3)
    , m_d3d12PipelineState(nullptr)
    , m_VS(nullptr)
    , m_PS(nullptr)
    , m_pEnvironmentConstant{ std::make_unique<EnvironmentConstant>() }
    , m_pEnvironmentPassConstant(nullptr)
{
    {
        auto device = Application::GetApp()->GetDevice();
        D3D12_FEATURE_DATA_ROOT_SIGNATURE sigVersion = {};
        sigVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &sigVersion, sizeof(sigVersion))))
        {
            sigVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }
        //Create root signature
        CD3DX12_DESCRIPTOR_RANGE1 descriptorRange = {};
        descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        //
        CD3DX12_ROOT_PARAMETER1 RootSignatures[EnvirRootParameters::NumEnvirRootParameters] = {};
        RootSignatures[EnvirRootParameters::ConstantBuffer].InitAsConstantBufferView(0);
        RootSignatures[EnvirRootParameters::ConstantPassBuffer].InitAsConstantBufferView(1);
        RootSignatures[EnvirRootParameters::EnvironmentMaps].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
        //
        CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(0);
        //
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSigDesc = {};
        //
        D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        versionedRootSigDesc.Init_1_1(EnvirRootParameters::NumEnvirRootParameters, RootSignatures, 1, &anisotropicWrap, Flags);
        //
        m_pRootSignature->SetRootSignatureDesc(versionedRootSigDesc.Desc_1_1, sigVersion.HighestVersion);
        //--------------------------------------------------------------------------------------------------------------------
        //
        m_VS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Environment.hlsl", nullptr, "VS", "vs_5_1");
        m_PS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Environment.hlsl", nullptr, "PS", "ps_5_1");

        CD3DX12_DEPTH_STENCIL_DESC DepthStencilDesc(D3D12_DEFAULT);
        DepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        DepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        CD3DX12_RASTERIZER_DESC RasterizerDesc(D3D12_DEFAULT);
        RasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

        D3D12_RT_FORMAT_ARRAY RTArray = {};
        RTArray.NumRenderTargets = 1;
        RTArray.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        DXGI_SAMPLE_DESC SampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(RTArray.RTFormats[0], Application::GetApp()->m_MultiSampleCount);

        struct EnvironmentPipelineState
        {
            CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendDesc;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL         DepthStencil;
            CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargets;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DepthStencilFormat;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveType;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
        }environmentPipelineState;
        environmentPipelineState.BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        environmentPipelineState.DepthStencil = DepthStencilDesc;
        environmentPipelineState.Rasterizer = RasterizerDesc;
        environmentPipelineState.pRootSignature = m_pRootSignature->GetRootSignature().Get();
        environmentPipelineState.VS = CD3DX12_SHADER_BYTECODE(m_VS.Get());
        environmentPipelineState.PS = CD3DX12_SHADER_BYTECODE(m_PS.Get());
        environmentPipelineState.RenderTargets = RTArray;
        environmentPipelineState.DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        environmentPipelineState.PrimitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        environmentPipelineState.InputLayout = { VertexPositionNormalTexture::InputElements,VertexPositionNormalTexture::InputElementCount };
        environmentPipelineState.SampleDesc = SampleDesc;

        D3D12_PIPELINE_STATE_STREAM_DESC pipelineDesc = { sizeof(environmentPipelineState),&environmentPipelineState };
        ThrowIfFailed(device->CreatePipelineState(&pipelineDesc, IID_PPV_ARGS(&m_d3d12PipelineState)));
        //Load Default environment maps
        std::shared_ptr<Texture> m_Environments0 = std::make_shared<Texture>();
        std::shared_ptr<Texture> m_Environments1 = std::make_shared<Texture>();
        std::shared_ptr<Texture> m_Environments2 = std::make_shared<Texture>();
        std::shared_ptr<Texture> m_Environments3 = std::make_shared<Texture>();
        commandList->LoadTextureFromFile(m_Environments0.get(), L"..\\NeoEngine\\Textures\\CubeMaps\\CornellBox.dds", TextureUsage::EnvironmentMap,true);
        commandList->LoadTextureFromFile(m_Environments1.get(), L"..\\NeoEngine\\Textures\\CubeMaps\\GraceCathedral.dds", TextureUsage::EnvironmentMap,true);
        commandList->LoadTextureFromFile(m_Environments2.get(), L"..\\NeoEngine\\Textures\\CubeMaps\\Indoor.dds", TextureUsage::EnvironmentMap,true);
        commandList->LoadTextureFromFile(m_Environments3.get(), L"..\\NeoEngine\\Textures\\CubeMaps\\SkyBox.dds", TextureUsage::EnvironmentMap,true);

        m_EnvironmentArray.push_back(m_Environments0);
        m_EnvironmentArray.push_back(m_Environments1);
        m_EnvironmentArray.push_back(m_Environments2);
        m_EnvironmentArray.push_back(m_Environments3);
        m_EnvironmentMaps.insert({ L"CornellBox.dds",std::move(m_Environments0) });
        m_EnvironmentMaps.insert({ L"GraceCathedral.dds",std::move(m_Environments1) });
        m_EnvironmentMaps.insert({ L"Indoor.dds",std::move(m_Environments2) });
        m_EnvironmentMaps.insert({ L"SkyBox.dds",std::move(m_Environments3) });
        //Fill environment constant buffer
        DirectX::XMStoreFloat4x4(&m_pEnvironmentConstant->World, DirectX::XMMatrixScaling(10000.0f, 10000.0f, 10000.0f));
    }
}


std::wstring Environment::AddNewEnvironmentMap(const std::wstring& filepath, std::shared_ptr<CommandList> commandList)
{
    assert(m_EnvironmentMaps.size() < m_MaxEnvironmentNum && "You can add six additional environmnet maps at most!");

    std::shared_ptr<Texture> pTexture = std::make_shared<Texture>();
    commandList->LoadTextureFromFile(pTexture.get(), filepath, TextureUsage::Diffuse);

    assert(pTexture->GetD3D12ResourceDesc().DepthOrArraySize == 6 && "Environment textures must be cubemap!");

    std::wstring name = filepath.substr(filepath.find_last_of(L"\\"));
    m_EnvironmentArray.push_back(pTexture);
    m_EnvironmentMaps.insert({ name,std::move(pTexture) });

    return name;
}

void Environment::EraseEnvironmentMap(const std::wstring& mapName)
{
    auto iterPos = m_EnvironmentMaps.find(mapName);
    if (iterPos != m_EnvironmentMaps.end())
    {
        m_EnvironmentMaps.erase(iterPos);
        auto pos = std::find(m_EnvironmentArray.begin(), m_EnvironmentArray.end(), iterPos->second);
        m_EnvironmentArray.erase(pos);
    }
}

void Environment::Create(std::shared_ptr<CommandList> commandList)
{
    assert(ms_pEnvironmnet == nullptr && "Environmnet has been created!");
    ms_pEnvironmnet = new Environment(commandList);
}

void Environment::Destroy()
{
    m_EnvironmentMaps.clear();
    m_EnvironmentArray.clear();

    if (ms_pEnvironmnet)
    {
        delete ms_pEnvironmnet;
        ms_pEnvironmnet = nullptr;
    }
}

Environment::~Environment() {};

void Environment::RenderEnvironment(std::shared_ptr<CommandList> commandList, const std::wstring& mapName)
{
    assert(m_EnvironmentMaps.size() && "There is no environment map!");
    if (mapName != L"")
    {
        auto iterPos = m_EnvironmentMaps.find(mapName);
        if (iterPos == m_EnvironmentMaps.end())
            assert("The environment map which you want to use is not found!");
        auto pos = std::find(m_EnvironmentArray.begin(), m_EnvironmentArray.end(), iterPos->second);
        assert(pos != m_EnvironmentArray.end() && "Error!");
        //refresh current environment index
        m_CurrentEnvironmentIndex = pos - m_EnvironmentArray.begin();
    }
    //
    commandList->SetD3D12PipelineState(m_d3d12PipelineState);
    commandList->SetGraphicsRootSignature(m_pRootSignature.get());

    D3D12_SHADER_RESOURCE_VIEW_DESC EnvironmentSrvDesc = {};
    EnvironmentSrvDesc.Format = m_EnvironmentArray[m_CurrentEnvironmentIndex]->GetD3D12ResourceDesc().Format;
    EnvironmentSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    EnvironmentSrvDesc.TextureCube.MipLevels = -1;
    EnvironmentSrvDesc.TextureCube.MostDetailedMip = 0;
    EnvironmentSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

    commandList->SetGraphicsDynamicConstantBuffer(EnvirRootParameters::ConstantBuffer, *m_pEnvironmentConstant);
    commandList->SetGraphicsDynamicConstantBuffer(EnvirRootParameters::ConstantPassBuffer, *m_pEnvironmentPassConstant);
    commandList->SetShaderResourceView(
        EnvirRootParameters::EnvironmentMaps,
        0,
        m_EnvironmentArray[m_CurrentEnvironmentIndex].get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        0,
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        &EnvironmentSrvDesc);

    m_SkyMesh->Draw(*commandList);
}

void Environment::SetEnvironmentConstantBuffer(EnvironmentPassConstant* EnvironmentPassCb)
{
    m_pEnvironmentPassConstant = EnvironmentPassCb;
}