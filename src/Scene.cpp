#include "Scene.h"
#include "Model.h"
#include "RootSignature.h"
#include "Application.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "FrustumCulling.h"
#include "Light.h"

#include <memory>

Scene* Scene::ms_pScene = nullptr;
std::unordered_map<std::string, std::unique_ptr<Model>> Scene::m_SceneModelsMap;
std::vector<std::unique_ptr<DirectionLight>> Scene::m_SceneDirectionLights;
std::vector<std::unique_ptr<SpotLight>> Scene::m_SceneSpotLights;
std::vector<std::unique_ptr<PointLight>> Scene::m_ScenePointLights;

Scene::Scene()
    :m_pFrustumCullinger(std::make_unique<FrustumCullinger>())
    ,m_IsDirtyScene(true)
{
    auto device = Application::GetApp()->GetDevice();
    //---------------------------------------------------------------------------------------------------------
    //For render scene aabb
    D3D12_FEATURE_DATA_ROOT_SIGNATURE highestVersion = {};
    highestVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &highestVersion, sizeof(highestVersion))))
    {
        highestVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_ROOT_PARAMETER1 rootParameter;
    rootParameter.InitAsConstantBufferView(0, 0);

    D3D12_ROOT_SIGNATURE_FLAGS Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionDesc = {};
    versionDesc.Init_1_1(1, &rootParameter, 0, nullptr, Flags);

    m_pRenderAABBRootSignature = std::make_unique<RootSignature>(versionDesc.Desc_1_1, highestVersion.HighestVersion);
    //
    m_AABBVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\AABB.hlsl", nullptr, "VS", "vs_5_1");
    m_AABBGS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\AABB.hlsl", nullptr, "GS", "gs_5_1");
    m_AABBPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\AABB.hlsl", nullptr, "PS", "ps_5_1");

    CD3DX12_DEPTH_STENCIL_DESC depthstencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depthstencilDesc.DepthEnable = true;
    depthstencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };
    //
    D3D12_RT_FORMAT_ARRAY RtArray = {};
    RtArray.NumRenderTargets = 1;
    RtArray.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    ScenePipelineState AABBpipelineDesc;
    AABBpipelineDesc.VS = CD3DX12_SHADER_BYTECODE(m_AABBVS.Get());
    AABBpipelineDesc.GS = CD3DX12_SHADER_BYTECODE(m_AABBGS.Get());
    AABBpipelineDesc.PS = CD3DX12_SHADER_BYTECODE(m_AABBPS.Get());
    AABBpipelineDesc.InputLayout = { layout,1 };
    AABBpipelineDesc.PrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    AABBpipelineDesc.pRootSignature = m_pRenderAABBRootSignature->GetRootSignature().Get();
    AABBpipelineDesc.BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    AABBpipelineDesc.DepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    AABBpipelineDesc.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    AABBpipelineDesc.SampleDesc = { 1,0 };
    AABBpipelineDesc.RenderTargetFormats = RtArray;

    D3D12_PIPELINE_STATE_STREAM_DESC streamDescAABB = { sizeof(AABBpipelineDesc),&AABBpipelineDesc };

    ThrowIfFailed(device->CreatePipelineState(&streamDescAABB, IID_PPV_ARGS(&m_d3d12RenderAABBPipelineState)));
}

Scene::~Scene()
{
}

void Scene::Create()
{
    assert(!ms_pScene && "Scene has been initilized");
    ms_pScene = new Scene;
}

void Scene::Destroy()
{
    m_SceneModelsMap.clear();
    m_SceneDirectionLights.clear();
    m_SceneSpotLights.clear();
    m_ScenePointLights.clear();

    if (ms_pScene)
    {
        assert(m_SceneModelsMap.size() == 0 && "Before destroy Scene,the static global variable must be clear!");

        delete ms_pScene;
        ms_pScene = nullptr;
    }
}

void Scene::DestroyMessageFromModel(const std::string& modelname)
{
    m_SceneModelsMap.erase(modelname);
    //when a model is deleted,we need to refresh aabb
    //RecreateSceneAABB();
}

Scene* Scene::GetScene()
{
    assert(ms_pScene && "Scene need to be initilized firstly");
    return ms_pScene;
}

bool Scene::GetSceneState()
{
    return ms_pScene;
}

std::string Scene::LoadModelFromFilePath(const std::string& Path,std::shared_ptr<CommandList> commandList)
{
    std::unique_ptr<Model> model = std::make_unique<Model>(ms_pScene);
    model->LoadModelFromFilePath(Path,commandList);

    std::string name = model->ModelName();
    m_SceneModelsMap.insert({ model->ModelName(),std::move(model) });

    //reset scene state
    Scene::GetScene()->m_IsDirtyScene = true;

    return name;
}

std::vector<std::string> Scene::LoadModelFromFilePaths(const std::vector<std::string>& Paths,std::shared_ptr<CommandList> commandList)
{
    assert(Paths.size() && "Error!Paths array can not be empty!");
    std::vector<std::string> names;
    for (const auto& path : Paths)
    {
        names.push_back(LoadModelFromFilePath(path, commandList));
    }
    return names;
}

const std::vector<const Model*> Scene::GetTypedModels()const
{
    std::vector<const Model*> models;

    for (const auto& model : m_SceneModelsMap)
    {
        models.push_back(model.second.get());
    }
    return models;
}

void Scene::SetWorldMatrix(const DirectX::CXMMATRIX& World, const std::string& ModelName /* = "" */)
{
    if (ModelName == "")
    {
        for (auto& model : m_SceneModelsMap)
        {
            model.second->SetWorldMatrix(World);
        }
    }
    else
    {
        auto iterPos = m_SceneModelsMap.find(ModelName);
        if (iterPos != m_SceneModelsMap.end())
        {
            iterPos->second->SetWorldMatrix(World);
        }
    }
}

void Scene::SetTexTransform(const DirectX::CXMMATRIX& TexTransform, const std::string& ModelName /* = "" */)
{
    if (ModelName == "")
    {
        for (auto& model : m_SceneModelsMap)
        {
            model.second->SetTexTransform(TexTransform);
        }
    }
    else
    {
        auto iterPos = m_SceneModelsMap.find(ModelName);
        if (iterPos != m_SceneModelsMap.end())
        {
            iterPos->second->SetTexTransform(TexTransform);
        }
    }
}

void Scene::SetMatTransform(const DirectX::CXMMATRIX& MatTransform, const std::string& ModelName /* = "" */)
{
    if (ModelName == "")
    {
        for (auto& model : m_SceneModelsMap)
        {
            model.second->SetMatTransform(MatTransform);
        }
    }
    else
    {
        auto iterPos = m_SceneModelsMap.find(ModelName);
        if (iterPos != m_SceneModelsMap.end())
        {
            iterPos->second->SetMatTransform(MatTransform);
        }
    }
}

DirectX::BoundingBox Scene::GetSceneBoundingBox()
{
    if (m_IsDirtyScene)
    {
        for (auto iter = m_SceneModelsMap.begin(); iter != m_SceneModelsMap.end(); ++iter)
        {
            //when first time create bounding box,we need to use first mesh to create first sub bounding box.
            if (iter == m_SceneModelsMap.begin())
            {
                auto mesh = iter->second->m_ModelLoader->Meshes()[0];
                std::vector<DirectX::XMFLOAT3> verticeInWorld;
                verticeInWorld.resize(mesh.mVertices.size());

                for (size_t i = 0; i < mesh.mVertices.size() ; ++i)
                {
                    DirectX::XMStoreFloat3(&verticeInWorld[i],DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&mesh.mVertices[i].Position), DirectX::XMLoadFloat4x4(&iter->second->GetWorldMatrix4x4f())));
                }

                DirectX::BoundingBox::CreateFromPoints(m_SceneBoundingBox, verticeInWorld.size(), verticeInWorld.data(), sizeof(DirectX::XMFLOAT3));
            }

            auto modelAABB = iter->second->BoundingBox();
            auto& modelWorld = iter->second->GetWorldMatrix4x4f();
            //transform local aabb to world space
            modelAABB.Transform(modelAABB, DirectX::XMLoadFloat4x4(&modelWorld));

            m_SceneBoundingBox.CreateMerged(m_SceneBoundingBox, m_SceneBoundingBox, modelAABB);
        }
    }
    return m_SceneBoundingBox;
}

void Scene::RenderSceneAABB(std::shared_ptr<CommandList> commandList, const Camera* pCamera)
{
    commandList->SetD3D12PipelineState(m_d3d12RenderAABBPipelineState);
    commandList->SetGraphicsRootSignature(m_pRenderAABBRootSignature.get());

    auto sceneAABB = GetSceneBoundingBox();

    RenderAABBCb renderCb;
    renderCb.Extents = sceneAABB.Extents;
    DirectX::XMStoreFloat4x4(&renderCb.ViewProj, DirectX::XMMatrixTranspose(pCamera->GetViewProj()));

    commandList->SetGraphicsDynamicConstantBuffer(0, renderCb);
    commandList->SetDynamicVertexBuffer(0, 1, sizeof(DirectX::XMFLOAT3), &sceneAABB.Center);

    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    commandList->Draw(1, 1, 0, 0);

    //then we render all model aabb
    for (const auto& model : m_SceneModelsMap)
    {
        model.second->RenderAABB(commandList, pCamera);
    }
}

DirectionLight* Scene::AddDirectionalLight(DirectX::XMFLOAT4 Strength, DirectX::XMFLOAT3 Direction, const std::string& LightName /* = "DirectionalLight" */, int ShadowSize /* = 1024 */, ShadowTechnology Technology /* = ShadowTechnology::StandardShadowMap */,const Camera* pMainCamera)
{
    m_SceneDirectionLights.push_back(std::make_unique<DirectionLight>(Strength, Direction, LightName, ShadowSize, Technology, pMainCamera));
    return m_SceneDirectionLights.back().get();
}

SpotLight* Scene::AddSpotLight(DirectX::XMFLOAT4 Strength /* =  */, DirectX::XMFLOAT3 Direction /* =  */, DirectX::XMFLOAT4 Position /* =  */, float Range /* = 10.0f */, float CosTheta /* = 0.5 */, const std::string& LightName /* = "SpotLight" */, int ShadowSize /* = 1024 */, ShadowTechnology Technology /* = ShadowTechnology::StandardShadowMap */)
{
    m_SceneSpotLights.push_back(std::make_unique<SpotLight>(Strength, Direction, Position, Range, CosTheta, LightName, ShadowSize, Technology));
    return m_SceneSpotLights.back().get();
}

PointLight* Scene::AddPointLight(const std::string& LightName /* = "PointLight" */, DirectX::XMFLOAT4 Strength /* =  */, DirectX::XMFLOAT4 Position /* =  */, float Radius /* = 10.0f */, int ShadowSize /* = 1024 */, ShadowTechnology Technology /* = ShadowTechnology::StandardShadowMap */)
{
    m_ScenePointLights.push_back(std::make_unique<PointLight>(Strength, Position, Radius, LightName, ShadowSize, Technology));
    return m_ScenePointLights.back().get();
}

const std::vector<LightConstants> Scene::GetSceneLightConstants()const
{
    std::vector<LightConstants> constants;
    for (const auto& directionlight : m_SceneDirectionLights)
    {
        constants.push_back(directionlight->GetLightConstant());
    }
    for (const auto& spotlight : m_SceneSpotLights)
    {
        constants.push_back(spotlight->GetLightConstant());
    }
    for (const auto& pointlight : m_ScenePointLights)
    {
        constants.push_back(pointlight->GetLightConstant());
    }
    return constants;
}