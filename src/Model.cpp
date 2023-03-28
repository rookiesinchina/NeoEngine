#include "Model.h"
#include "d3dUtil.h"
#include "CommandList.h"
#include "Application.h"
#include "CommandQueue.h"
#include "Texture.h"
#include "Pass.h"
#include "Camera.h"

Model::Model(Scene* pScene)
    :m_ModelName("NoName")
    ,m_ModelLoader(nullptr)
    ,m_pVertexBuffer(nullptr)
    ,m_pIndexBuffer(nullptr)
    ,m_ModelWorld(MathHelper::Identity4x4())
{
    auto device = Application::GetApp()->GetDevice();
    //Create default SRV
    for (int i = 0; i < TextureUsage::NumTextureUsage; ++i)
    {
        m_DefaultSRV[i] = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_MaxTextureNum);

        D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
        SrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SrvDesc.Texture2D.MipLevels = 1;
        SrvDesc.Texture2D.MostDetailedMip = 0;
        SrvDesc.Texture2D.PlaneSlice = 0;
        SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

        for (int num = 0; num < m_MaxTextureNum; ++num)
        {
            device->CreateShaderResourceView(nullptr, &SrvDesc, m_DefaultSRV[i].GetDescriptorHandle(num));
        }
    }
}

Model::~Model()
{
}

void Model::Destroy()
{
    Scene::GetScene()->DestroyMessageFromModel(m_ModelName);
}

//std::vector<UINT> Model::SetMoreRootSignatureOnDefault(const std::vector<CD3DX12_ROOT_PARAMETER1>& RootParameters)
//{
//    assert(SceneRootParameter::NumRootParameters + RootParameters.size() <= Scene::m_MaxNumRootParameters && "The number of additional root parameters can not exceed 12");
//
//    std::vector<UINT> additionalRootIndex;
//    if (m_pRootSignature)
//    {
//        //Create new root parameters
//        auto newRootParameters = Scene::GetScene()->GetDefaultRootParameter();
//
//        for (size_t i = 0; i < RootParameters.size(); ++i)
//        {
//            newRootParameters[SceneRootParameter::NumRootParameters + i] = RootParameters[i];
//
//            if (RootParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
//            {
//                D3D12_DESCRIPTOR_RANGE1* descriptorRange = RootParameters[i].DescriptorTable.NumDescriptorRanges > 0 ?
//                    new D3D12_DESCRIPTOR_RANGE1[RootParameters[i].DescriptorTable.NumDescriptorRanges] : nullptr;
//
//                memcpy(descriptorRange, RootParameters[i].DescriptorTable.pDescriptorRanges, sizeof(D3D12_DESCRIPTOR_RANGE1) * RootParameters[i].DescriptorTable.NumDescriptorRanges);
//
//                newRootParameters[SceneRootParameter::NumRootParameters + i].DescriptorTable.pDescriptorRanges = descriptorRange;
//                newRootParameters[SceneRootParameter::NumRootParameters + i].DescriptorTable.NumDescriptorRanges = RootParameters[i].DescriptorTable.NumDescriptorRanges;
//            }
//
//            additionalRootIndex.push_back(SceneRootParameter::NumRootParameters + i);
//        }
//        //Create new root signature desc
//        D3D12_FEATURE_DATA_ROOT_SIGNATURE rootVersion = {};
//        rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
//        if (FAILED(Application::GetApp()->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootVersion, sizeof(rootVersion))))
//        {
//            rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
//        }
//
//        auto staticSamplers = d3dUtil::GetStaticSamplers();
//
//        D3D12_ROOT_SIGNATURE_FLAGS Flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
//        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSigDesc = {};
//        versionRootSigDesc.Init_1_1(SceneRootParameter::NumRootParameters + RootParameters.size(), newRootParameters, staticSamplers.size(), staticSamplers.data(),Flag);
//        
//        m_pRootSignature.reset();
//        m_pRootSignature = std::make_shared<RootSignature>(versionRootSigDesc.Desc_1_1, rootVersion.HighestVersion);
//    }
//    return additionalRootIndex;

//}

//void Model::SetModelPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState)
//{
//    if (pipelineState)
//    {
//        m_d3d12PipelineState = pipelineState;
//    }
//}

void Model::LoadModelFromFilePath(const std::string& FilePath,std::shared_ptr<CommandList> commandList)
{
    m_ModelLoader = std::make_unique<ModelSpace::ModelLoader>(FilePath);

    auto meshes = m_ModelLoader->Meshes();
    m_ModelName = m_ModelLoader->ModelName();

    uint32_t curMaterialIndex = 0;
    Material     meshMaterial;
    MeshConstant meshConstant;

    //Set a default material
    m_MeshMaterials.push_back(Material());
    for (size_t i  = 0 ; i < meshes.size() ; ++i)
    {
        if (meshes[i].mbHasMaterial)
        {
            //We first fill material texture index
            for (int j = 0; j < TextureUsage::NumTextureUsage; ++j)
            {
                auto textureIndex = m_ModelLoader->GetTextureMapIndex(static_cast<TextureUsage>(j));
                auto iterPos = textureIndex.find(meshes[i].mMeshName);
                if (iterPos != textureIndex.end())
                {
                    switch (static_cast<TextureUsage>(j))
                    {
                    case Diffuse:
                        meshMaterial.DiffuseTextureIndex = iterPos->second;
                        break;
                    case Specular:
                        meshMaterial.SpecularTextureIndex = iterPos->second;
                        break;
                    case HeightMap:
                        meshMaterial.HeightTextureIndex = iterPos->second;
                        break;
                    case NormalMap:
                        meshMaterial.NormalTextureIndex = iterPos->second;
                        break;
                    case Ambient:
                        meshMaterial.AmbientTextureIndex = iterPos->second;
                        break;
                    case Opacity:
                        meshMaterial.OpacityTextureIndex = iterPos->second;
                        break;
                    case Emissive:
                        meshMaterial.EmissiveTextureIndex = iterPos->second;
                        break;
                    }
                }
            }
            //then we fill material parameters
            meshMaterial.DiffuseColor = meshes[i].mMeshMaterial.DiffuseColor;
            meshMaterial.SpecularColor = meshes[i].mMeshMaterial.SpecularColor;
            meshMaterial.AmbientColor = meshes[i].mMeshMaterial.AmbientColor;
            meshMaterial.EmissiveColor = meshes[i].mMeshMaterial.EmissiveColor;
            //reserved for future....

            m_MeshMaterials.push_back(meshMaterial);
            meshConstant.MaterialIndex = ++curMaterialIndex;
        }
        else//If a mesh has not mtl file....we give him a defualt material.
        {
            meshConstant.MaterialIndex = 0;
        }
        //then we fill mesh constant
        meshConstant.WorldMatrix = MathHelper::Identity4x4();
        meshConstant.TexTransform = MathHelper::Identity4x4();
        m_MeshConstants.push_back(meshConstant);
        //Finally,we create model AABB
        if (i == 0)
        {
            DirectX::BoundingBox::CreateFromPoints(m_ModelAABB, meshes[i].mVertices.size(), (DirectX::XMFLOAT3*)(meshes[i].mVertices.data()), sizeof(ModelSpace::Vertex));
        }
        DirectX::BoundingBox::CreateMerged(m_ModelAABB, m_ModelAABB, meshes[i].mMeshAABB);
    }

    //After initilize mesh constant and materials,we need to load texture immediately
    LoadModelTexture(commandList);
    //then we create vertex and index buffer
    SetVertexAndIndexBuffer(commandList);
}

void Model::SetVertexAndIndexBuffer(std::shared_ptr<CommandList> commandList)
{
    m_pVertexBuffer = std::make_unique<VertexBuffer>(AnsiToWString(m_ModelName) + L" Vertex Buffer");
    m_pIndexBuffer = std::make_unique<IndexBuffer>(AnsiToWString(m_ModelName) + L" Index Buffer");
    //merging all vertice and indice of meshes to one buffer. 
    std::vector<ModelSpace::Vertex> vertices;
    std::vector<uint32_t>indices;
    for (const auto& mesh : m_ModelLoader->Meshes())
    {
        for (const auto& v : mesh.mVertices)
        {
            vertices.push_back(v);
        }
        for (const auto& i : mesh.mIndices)
        {
            indices.push_back(i);
        }
    }
    commandList->CopyVertexBuffer(m_pVertexBuffer.get(), vertices);
    commandList->CopyIndexBuffer(m_pIndexBuffer.get(), indices);
}

void Model::SetWorldMatrix(const DirectX::CXMMATRIX& World)
{
    assert(m_ModelLoader &&  m_MeshConstants.size() &&"Set Model Firstly Or Mesh Constant is empty");
    auto meshes = m_ModelLoader->Meshes();
    for (size_t i = 0 ; i < meshes.size() ; ++i)
    {
        DirectX::XMStoreFloat4x4(&m_MeshConstants[i].WorldMatrix, DirectX::XMMatrixTranspose(World));
    }
    DirectX::XMStoreFloat4x4(&m_ModelWorld, World);
}

void Model::SetTexTransform(const DirectX::CXMMATRIX& TexTransform)
{
    assert(m_ModelLoader && m_MeshConstants.size() && "Set Model Firstly Or Mesh Constant is empty");
    auto meshes = m_ModelLoader->Meshes();
    for (size_t i = 0; i < meshes.size(); ++i)
    {
        DirectX::XMStoreFloat4x4(&m_MeshConstants[i].TexTransform, DirectX::XMMatrixTranspose(TexTransform));
    }
}

void Model::SetMatTransform(const DirectX::CXMMATRIX& MatTransform)
{
    assert(m_ModelLoader && m_MeshMaterials.size() && "Set Model Firstly Or Mesh Material is empty");
    auto meshes = m_ModelLoader->Meshes();
    for (size_t i = 0; i < meshes.size(); ++i)
    {
        DirectX::XMStoreFloat4x4(&m_MeshMaterials[i].MatTransform, DirectX::XMMatrixTranspose(MatTransform));
    }
}

void Model::LoadModelTexture(std::shared_ptr<CommandList> commandList)
{
    for (int i = 0; i < TextureUsage::NumTextureUsage; ++i)
    {
        auto texturePaths = m_ModelLoader->GetTextureMapPath(static_cast<TextureUsage>(i));
        for (const auto& path : texturePaths)
        {
            std::unique_ptr<Texture> pTexture = std::make_unique<Texture>();
            commandList->LoadTextureFromFile(pTexture.get(), AnsiToWString(m_ModelLoader->Directory()) + AnsiToWString(path), static_cast<TextureUsage>(i));
            m_pTexture[i].push_back(std::move(pTexture));
        }
    }
}

void Model::RenderAABB(std::shared_ptr<CommandList> commandList,const Camera* pCamera)
{
    //Since Scene::RenderSceneAABB has set pipeline state and root signature.So there is no need to set again.
    
    //commandList->SetD3D12PipelineState(m_pScene->m_d3d12RenderAABBPipelineState);
    //commandList->SetGraphicsRootSignature(m_pScene->m_pRenderAABBRootSignature.get());
    //Fill constant buffer
    
    Scene::RenderAABBCb renderCb;

    DirectX::BoundingBox aabb;
    m_ModelAABB.Transform(aabb, DirectX::XMLoadFloat4x4(&m_ModelWorld));

    renderCb.Extents = aabb.Extents;
    DirectX::XMStoreFloat4x4(&renderCb.ViewProj, DirectX::XMMatrixTranspose(pCamera->GetViewProj()));

    commandList->SetGraphicsDynamicConstantBuffer(0, renderCb);
    //for rendering aabb, we just need to draw a null 
    commandList->SetDynamicVertexBuffer(0, 1, sizeof(DirectX::XMFLOAT3), &aabb.Center);
    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    commandList->Draw(1, 1, 0, 0);

    //for (auto& mesh : m_ModelLoader->Meshes())
    //{
    //    Scene::RenderAABBCb renderCb;

    //    DirectX::BoundingBox aabb;
    //    mesh.mMeshAABB.Transform(aabb, DirectX::XMLoadFloat4x4(&m_ModelWorld));

    //    renderCb.Extents = aabb.Extents;
    //    renderCb.ViewProj = Scene::GetScene()->m_pPassConstant->ViewProj;

    //    commandList->SetGraphicsDynamicConstantBuffer(0, renderCb);
    //    //for rendering aabb, we just need to draw a null 
    //    commandList->SetDynamicVertexBuffer(0, 1, sizeof(DirectX::XMFLOAT3), &aabb.Center);
    //    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    //    commandList->Draw(1, 1, 0, 0);
    //}
}