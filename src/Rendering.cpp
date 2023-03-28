#include "Rendering.h"
#include "Application.h"
#include "RootSignature.h"
#include "CommandList.h"
#include "Camera.h"
#include "FrustumCulling.h"
#include "DynamicDescriptorHeap.h"

/************************************************************************/
/*Following functions are for forward rendering.                                 
/************************************************************************/

ForwardRendering::ForwardRendering(ForwardPassType Type, std::shared_ptr<RenderTarget> pRenderTarget, const Camera* pOutputCamera)
    :PassBase(pRenderTarget, pOutputCamera)
    , m_ForwardType(Type)
    , m_pForwardShdaowPass(std::make_unique<ShadowPass>())
{
    //Create default pipeline and root signature for forward rendering.
    auto device = Application::GetApp()->GetDevice();
    //Then we create root signature.
    m_pRootSignature = std::make_shared<RootSignature>();

    D3D12_FEATURE_DATA_ROOT_SIGNATURE highestVersion = {};
    highestVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &highestVersion, sizeof(highestVersion))))
    {
        highestVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    //
    CD3DX12_DESCRIPTOR_RANGE1 diffuse = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxTextureNum,0,2 };
    CD3DX12_DESCRIPTOR_RANGE1 specular = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxTextureNum,0,3 };
    CD3DX12_DESCRIPTOR_RANGE1 height = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxTextureNum,0,4 };
    CD3DX12_DESCRIPTOR_RANGE1 normal = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxTextureNum,0,5 };
    CD3DX12_DESCRIPTOR_RANGE1 ambient = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxTextureNum,0,6 };
    CD3DX12_DESCRIPTOR_RANGE1 opacity = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxTextureNum,0,7 };
    CD3DX12_DESCRIPTOR_RANGE1 emissive = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxTextureNum,0,8 };

    CD3DX12_DESCRIPTOR_RANGE1 directionspotshadow = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxDirectionAndSpotLightShadowNum,0,12 };
    CD3DX12_DESCRIPTOR_RANGE1 pointshadow = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,m_MaxPointLightShadowNum,0,13 };

    CD3DX12_ROOT_PARAMETER1 RootParameters[RenderingRootParameter::NumRootParameters];
    RootParameters[RenderingRootParameter::MeshConstantCB].InitAsConstantBufferView(0);
    RootParameters[RenderingRootParameter::PassConstantCB].InitAsConstantBufferView(1);
    RootParameters[RenderingRootParameter::StructuredLight].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    RootParameters[RenderingRootParameter::StructuredMaterials].InitAsShaderResourceView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    RootParameters[RenderingRootParameter::DiffuseTexture].InitAsDescriptorTable(1, &diffuse, D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[RenderingRootParameter::SpecularTexture].InitAsDescriptorTable(1, &specular, D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[RenderingRootParameter::HeightTexture].InitAsDescriptorTable(1, &height, D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[RenderingRootParameter::NormalTexture].InitAsDescriptorTable(1, &normal, D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[RenderingRootParameter::AmbientTexture].InitAsDescriptorTable(1, &ambient, D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[RenderingRootParameter::OpacityTexture].InitAsDescriptorTable(1, &opacity, D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[RenderingRootParameter::EmissiveTexture].InitAsDescriptorTable(1, &emissive, D3D12_SHADER_VISIBILITY_PIXEL);

    RootParameters[RenderingRootParameter::DirectionAndSoptLightShadowTexture].InitAsDescriptorTable(1, &directionspotshadow, D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[RenderingRootParameter::PointLightShadowTexture].InitAsDescriptorTable(1, &pointshadow, D3D12_SHADER_VISIBILITY_PIXEL);
    //
    auto staticSamplers = d3dUtil::GetStaticSamplers();
    //
    D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSigDesc = {};
    versionedRootSigDesc.Init_1_1(RenderingRootParameter::NumRootParameters, RootParameters, staticSamplers.size(), staticSamplers.data(), Flags);
    //
    m_pRootSignature->SetRootSignatureDesc(versionedRootSigDesc.Desc_1_1, highestVersion.HighestVersion);
    //-------------------------------------------------------------------------------------------------------------
    //Compile default shader
    Microsoft::WRL::ComPtr<ID3DBlob> ForwardVS;
    Microsoft::WRL::ComPtr<ID3DBlob> ForwardPS;

    ForwardVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\ForwardRendering.hlsl", nullptr, "VS", "vs_5_1");
    ForwardPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\ForwardRendering.hlsl", nullptr, "PS", "ps_5_1");
    //
    DXGI_SAMPLE_DESC DefaultSampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(m_pRenderTarget->GetRenderTargetFormats().RTFormats[0], Application::GetApp()->m_MultiSampleCount);
    //Create Default pipeline state.
    PassPipelineState passPipelineState;

    CD3DX12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    //If this pass is transparent pass,we need to set blend state.
    if (m_ForwardType == ForwardPassType::TransparentPass)
    {
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    passPipelineState.BlendDesc = blendDesc;
    passPipelineState.DepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    passPipelineState.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    passPipelineState.VS = CD3DX12_SHADER_BYTECODE(ForwardVS.Get());
    passPipelineState.PS = CD3DX12_SHADER_BYTECODE(ForwardPS.Get());
    passPipelineState.RenderTargetFormats = m_pRenderTarget->GetRenderTargetFormats();
    passPipelineState.DepthStencilFormat = m_pRenderTarget->GetDepthStencilFormat();
    passPipelineState.InputLayout = { ModelSpace::ModelInputElemets,_countof(ModelSpace::ModelInputElemets) };
    passPipelineState.pRootSignature = m_pRootSignature->GetRootSignature().Get();
    passPipelineState.SampleDesc = DefaultSampleDesc;
    passPipelineState.PrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = { sizeof(passPipelineState),&passPipelineState };

    ThrowIfFailed(device->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_id3d12PassPipelineState)));

}

void ForwardRendering::ExecutePass(
    std::shared_ptr<CommandList> commandList,
    std::function<void()> SetResourceFunc)
{
    //Set shadow pass
    m_pForwardShdaowPass->ExecutePass(commandList);
    //If setResourceFunc is empty,then use default resource func.
    if (!SetResourceFunc)
    {
        SetResourceFunc = [&]()
        {
            for (const auto& pModel : m_pInputMoedels)
            {
                if (pModel)
                {
                    auto meshes = pModel->GetModelLoader()->Meshes();

                    m_pPassFrustumCullinger->BindModelCulled(pModel);

                    commandList->SetVertexBuffer(0, pModel->GetVertexBuffer());
                    commandList->SetIndexBuffer(pModel->GetIndexBuffer());
                    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                    commandList->SetGraphicsDynamicConstantBuffer(RenderingRootParameter::PassConstantCB, m_ForwardPassConstants);
                    commandList->SetGraphicsStructuredBuffer(RenderingRootParameter::StructuredLight, m_pForwardShdaowPass->GetLightConstants());
                    commandList->SetGraphicsStructuredBuffer(RenderingRootParameter::StructuredMaterials, pModel->GetMeshMaterials());
                    for (int usage = 0; usage < TextureUsage::NumTextureUsage; ++usage)
                    {
                        UINT rootParameterIndex = 0;
                        auto Usage = static_cast<TextureUsage>(usage);
                        switch (Usage)
                        {
                        case Diffuse:
                            rootParameterIndex = RenderingRootParameter::DiffuseTexture;
                            break;
                        case Specular:
                            rootParameterIndex = RenderingRootParameter::SpecularTexture;
                            break;
                        case HeightMap:
                            rootParameterIndex = RenderingRootParameter::HeightTexture;
                            break;
                        case NormalMap:
                            rootParameterIndex = RenderingRootParameter::NormalTexture;
                            break;
                        case Ambient:
                            rootParameterIndex = RenderingRootParameter::AmbientTexture;
                            break;
                        case Opacity:
                            rootParameterIndex = RenderingRootParameter::OpacityTexture;
                            break;
                        case Emissive:
                            rootParameterIndex = RenderingRootParameter::EmissiveTexture;
                            break;
                        default:
                            assert(FALSE && "Error!Unexpected texture usage!");
                            break;
                        }
                        //Bind textures into shaders.
                        for (size_t i = 0; i < pModel->GetTextures(Usage).size(); ++i)
                        {
                            commandList->SetShaderResourceView(rootParameterIndex, i, pModel->GetTextures(Usage)[i].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                        }
                        if (pModel->GetTextures(Usage).size() < m_MaxTextureNum)
                        {
                            commandList->GetDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->StageDescriptors(
                                pModel->GetDefaultSrvDescriptors(Usage).GetDescriptorHandle(),
                                rootParameterIndex,
                                pModel->GetTextures(Usage).size(),
                                m_MaxTextureNum - pModel->GetTextures(Usage).size());
                        }
                    }
                    //Set directional and spot shadow textures
                    auto DirectionalAndSpotShadow = m_pForwardShdaowPass->GetDirectionAndSpotShadows();
                    for (int i = 0; i < DirectionalAndSpotShadow.size(); ++i)
                    {
                        auto Desc = DirectionalAndSpotShadow[i]->GetD3D12ResourceDesc();
                        D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
                        SrvDesc.Format = Desc.Format;
                        SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        SrvDesc.Texture2DArray.ArraySize = Desc.DepthOrArraySize;
                        SrvDesc.Texture2DArray.FirstArraySlice = 0;
                        SrvDesc.Texture2DArray.MipLevels = 1;
                        SrvDesc.Texture2DArray.MostDetailedMip = 0;
                        SrvDesc.Texture2DArray.PlaneSlice = 0;
                        SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

                        commandList->SetShaderResourceView(
                            RenderingRootParameter::DirectionAndSoptLightShadowTexture, i,
                            DirectionalAndSpotShadow[i],
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
                    }
                    commandList->GetDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->StageDescriptors(
                        m_pForwardShdaowPass->GetDirectionAndSpotDefaultSrvDescriptors().GetDescriptorHandle(),
                        RenderingRootParameter::DirectionAndSoptLightShadowTexture,
                        DirectionalAndSpotShadow.size(),
                        m_MaxDirectionAndSpotLightShadowNum - DirectionalAndSpotShadow.size());
                    ////Set point shadow textures
                    auto PointShadows = m_pForwardShdaowPass->GetPointShadows();
                    for (int i = 0; i < PointShadows.size(); ++i)
                    {
                        auto Desc = PointShadows[i]->GetD3D12ResourceDesc();
                        //Point light shadow map is cube map.
                        D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
                        SrvDesc.Format = Desc.Format;
                        SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        SrvDesc.TextureCube.MipLevels = 1;
                        SrvDesc.TextureCube.MostDetailedMip = 0;
                        SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

                        commandList->SetShaderResourceView(
                            RenderingRootParameter::PointLightShadowTexture, i,
                            PointShadows[i],
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &SrvDesc);
                    }
                    commandList->GetDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->StageDescriptors(
                        m_pForwardShdaowPass->GetPointDefaultSrvDescriptors().GetDescriptorHandle(),
                        RenderingRootParameter::PointLightShadowTexture,
                        PointShadows.size(),
                        m_MaxPointLightShadowNum - PointShadows.size());
                    //After binding resources,we can begin to draw
                    for (size_t i = 0; i < meshes.size(); ++i)
                    {
                        //Check if this mesh is culled by frustum.
                        if (meshes[i].m_IsCulled == false)
                        {
                            //Bind each mesh resources to shader
                            auto meshconstantBuffer = pModel->GetMeshConstants();
                            commandList->SetGraphicsDynamicConstantBuffer(RenderingRootParameter::MeshConstantCB, meshconstantBuffer[i]);
                            commandList->DrawIndexed(meshes[i].mIndices.size(), 1, meshes[i].mIndexOffset, meshes[i].mVertexOffset, 0);
                        }
                    }
                }
            }
        };
    }
    PassBase::ExecutePass(commandList, SetResourceFunc);
    /**
     * Following functions are only for debugging.
     */
    if (FALSE)
    {
        Scene::GetScene()->RenderSceneAABB(commandList, m_pRenderingCamera);
        //render for light frustum
        for (const auto& directionlight : Scene::GetScene()->GetSceneDirectionalLights())
        {
            directionlight->RenderLightFrustum(commandList, m_pRenderingCamera);
        }
        for (const auto& spotlight : Scene::GetScene()->GetSceneSpotLights())
        {
            spotlight->RenderLightFrustum(commandList, m_pRenderingCamera);
        }
        for (const auto& pointlight : Scene::GetScene()->GetScenePointLights())
        {
            pointlight->RenderLightFrustum(commandList, m_pRenderingCamera);
        }
    }
}

void ForwardRendering::UpdatePass(const UpdateEventArgs& Args, std::function<void()> UpdateFunc /* = */)
{
    auto width = m_pRenderTarget->GetTexture(AttachmentPoint::Color0).GetD3D12ResourceDesc().Width;
    auto height = m_pRenderTarget->GetTexture(AttachmentPoint::Color0).GetD3D12ResourceDesc().Height;

    DirectX::XMStoreFloat4x4(&m_ForwardPassConstants.View, DirectX::XMMatrixTranspose(m_pRenderingCamera->GetView()));
    DirectX::XMStoreFloat4x4(&m_ForwardPassConstants.Proj, DirectX::XMMatrixTranspose(m_pRenderingCamera->GetProj()));
    DirectX::XMStoreFloat4x4(&m_ForwardPassConstants.InvView, DirectX::XMMatrixTranspose(m_pRenderingCamera->GetInvView()));
    DirectX::XMStoreFloat4x4(&m_ForwardPassConstants.InvProj, DirectX::XMMatrixTranspose(m_pRenderingCamera->GetInvProj()));
    DirectX::XMStoreFloat4x4(&m_ForwardPassConstants.ViewProj, DirectX::XMMatrixTranspose(m_pRenderingCamera->GetViewProj()));
    DirectX::XMStoreFloat4x4(&m_ForwardPassConstants.InvViewProj, DirectX::XMMatrixTranspose(m_pRenderingCamera->GetInvViewProj()));
    DirectX::XMStoreFloat3(&m_ForwardPassConstants.EyePosW, m_pRenderingCamera->GetPosition());
    m_ForwardPassConstants.RenderTargetSize = { (float)width,(float)height };
    m_ForwardPassConstants.InvRenderTargetSize = { 1.0f / (float)width , 1.0f / (float)height };
    m_ForwardPassConstants.ZNear = m_pRenderingCamera->GetNearZ();
    m_ForwardPassConstants.ZFar = m_pRenderingCamera->GetFarZ();
    m_ForwardPassConstants.DeltaTime = Args.ElapsedTime;
    m_ForwardPassConstants.TotalTime = Args.TotalTime;
    m_ForwardPassConstants.NumLights = Scene::GetNumLightsInScene();
    //update something if have.
    if (UpdateFunc)
    {
        UpdateFunc();
    }
}