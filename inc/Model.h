#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <string>
#include <unordered_map>
#include <memory>

#include "MathHelper.h"
#include "ModelLoader.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"
#include "DescriptorAllocation.h"
#include "RootSignature.h"
#include "d3dx12.h"


struct MeshConstant
{
    DirectX::XMFLOAT4X4 WorldMatrix = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    uint32_t            MaterialIndex = 0; //Material index,0 indicates that this mesh has a default material
    DirectX::XMUINT3    Padding0;
};

struct Material
{
    DirectX::XMFLOAT4 DiffuseColor = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT4 SpecularColor = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT4 AmbientColor = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT4 EmissiveColor = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 FresnelR0 = { 1.0f,1.0f,1.0f };
    float Roughness = 1.0f;
    float SpecularExponent = 20.0f;
    float SpecularScaling = 1.0f;
    float TransparentFactor = 1.0f;
    float IndexOfRefraction = 1.0f;
    //Texture indice,-1 means no specific texture
    int32_t DiffuseTextureIndex = -1;
    int32_t SpecularTextureIndex = -1;
    int32_t NormalTextureIndex = -1;
    int32_t HeightTextureIndex = -1;
    int32_t AmbientTextureIndex = -1;
    int32_t OpacityTextureIndex = -1;
    int32_t EmissiveTextureIndex = -1;
    //you can add other texture index 
};

/**
 * 
 */
class ModelLoader;
class VertexBuffer;
class IndexBuffer;
class Texture;
class Scene;
class RootSignature;
class CommandList;
class Camera;

class Model
{
public:
    Model(Scene* pScene);
    ~Model();

    void LoadModelFromFilePath(const std::string& FilePath,std::shared_ptr<CommandList> commandList);
    //Set world matirx for a specific mesh,if use default parameter which means set this world matrix to all meshes in this model.
    void SetWorldMatrix(const DirectX::CXMMATRIX& World);
    void SetTexTransform(const DirectX::CXMMATRIX& TexTransform);
    void SetMatTransform(const DirectX::CXMMATRIX& MatTransform);
    //
    const std::string& ModelName()const { return m_ModelName; }

    const DirectX::BoundingBox& BoundingBox()const { return m_ModelAABB; }

    const DirectX::XMFLOAT4X4& GetWorldMatrix4x4f()const { return m_ModelWorld;}
 
    void Destroy();
    //------------------------------------------------------------------------------------
    void RenderAABB(std::shared_ptr<CommandList> commandList, const Camera* pCamera);
    //------------------------------------------------------------------------------------
    const ModelSpace::ModelLoader* GetModelLoader()const { return m_ModelLoader.get(); }

    const VertexBuffer* GetVertexBuffer()const { return m_pVertexBuffer.get(); }

    const IndexBuffer* GetIndexBuffer()const { return m_pIndexBuffer.get(); }

    const std::vector<Material>& GetMeshMaterials()const { return m_MeshMaterials; }

    const std::vector<std::unique_ptr<Texture>>& GetTextures(TextureUsage Usage)const { return m_pTexture[Usage]; }

    const std::vector<MeshConstant>& GetMeshConstants()const { return m_MeshConstants; }

    const DescriptorAllocation& GetDefaultSrvDescriptors(TextureUsage Usage)const { return m_DefaultSRV[Usage]; }
protected:
    //
    void LoadModelTexture(std::shared_ptr<CommandList> commandList);
    //
    void SetVertexAndIndexBuffer(std::shared_ptr<CommandList> commandList);
private:
    friend class FrustumCullinger;
    friend class CommandList;
    friend class Scene;

    std::string m_ModelName;
    //using MeshRenderItem = std::unordered_map<std::string, RenderItem>;
    std::unique_ptr<ModelSpace::ModelLoader> m_ModelLoader;
    //Axis-Aligned BoundingBox for this model
    //Note:AABB is in local space of this model.
    DirectX::BoundingBox m_ModelAABB;
    DirectX::XMFLOAT4X4 m_ModelWorld;
    //Note:For now, we just set one same world matrix for all meshes in this model!
    std::vector<MeshConstant> m_MeshConstants;
    std::vector<Material> m_MeshMaterials;

    std::unique_ptr<VertexBuffer> m_pVertexBuffer;
    std::unique_ptr<IndexBuffer> m_pIndexBuffer;

    std::vector<std::unique_ptr<Texture>> m_pTexture[TextureUsage::NumTextureUsage];
    //Some default srv for DirectX happy.
    DescriptorAllocation m_DefaultSRV[TextureUsage::NumTextureUsage];
};