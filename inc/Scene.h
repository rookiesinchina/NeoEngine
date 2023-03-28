#pragma once
#include <array>
#include <unordered_map>
#include <memory>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <functional>

#include "d3dx12.h"
#include "DescriptorAllocation.h"
#include "Model.h"
#include "Light.h"
#include "FrustumCulling.h"


struct ScenePipelineState
{
    CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendDesc;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL         DepthStencil;
    CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
    CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
    CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
    CD3DX12_PIPELINE_STATE_STREAM_HS                    HS;
    CD3DX12_PIPELINE_STATE_STREAM_GS                    GS;
    CD3DX12_PIPELINE_STATE_STREAM_DS                    DS;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargetFormats;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DepthStencilFormat;
    CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
    CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
};


class RootSignature;
class CommandList;

class Scene
{
public:
    //Get scene state.If this scene has not been created,will return false.
    static bool GetSceneState();
    //
    static Scene* GetScene();
    //Create a scene to load all models
    //Note:When the scene is not used,you must use Scene::Destroy() method!
    static void Create();
    //Destroy Scene instance
    static void Destroy();
    //Load a model from file path and return this model name
    //Note:since load textures and vertex index buffer will use commandlist,we set this parameter here.
    //After loading models,do not forget to use CommandQueue::ExecuteCommandList() and CommandQueue::WaitForFenceValue() to wait commands complete. 
    static std::string LoadModelFromFilePath(const std::string& Path,std::shared_ptr<CommandList> commandList);
    static std::vector<std::string> LoadModelFromFilePaths(const std::vector<std::string>& Paths,std::shared_ptr<CommandList> commandList);
    //
    void SetWorldMatrix(const DirectX::CXMMATRIX& World, const std::string& ModelName = "");
    //
    void SetTexTransform(const DirectX::CXMMATRIX& TexTransform, const std::string& ModelName = "");
    //
    void SetMatTransform(const DirectX::CXMMATRIX& MatTransform, const std::string& ModelName = "");

    void DestroyMessageFromModel(const std::string& modelname);
    //
    DirectX::BoundingBox GetSceneBoundingBox();
    //
    static const std::unordered_map<std::string, std::unique_ptr<Model>>& GetSceneMap() { return m_SceneModelsMap; }

    const FrustumCullinger* GetSceneFrustumCullinger()const { return m_pFrustumCullinger.get(); }

    const std::vector<const Model*> GetTypedModels()const;

    void RenderSceneAABB(std::shared_ptr<CommandList> commandList, const Camera* pCamera);
    //----------------------------------------------------------------------------------------------------------------------
    //Following functions for lights
    //Add a specific light and set shadow type for this light.
    //Note:@param: pMainCamera is nullptr for shadow maps except cascaded shadow map!
    static DirectionLight* AddDirectionalLight(DirectX::XMFLOAT4 Strength = { 1.0f,1.0f,1.0f,1.0f }, DirectX::XMFLOAT3 Direction = {-1.0f,-1.0f,-1.0f}, const std::string& LightName = "DirectionalLight", int ShadowSize = 1024, ShadowTechnology Technology = ShadowTechnology::StandardShadowMap,const Camera* pMainCamera = nullptr);
    static SpotLight* AddSpotLight(DirectX::XMFLOAT4 Strength = { 1.0f,1.0f,1.0f,1.0f }, DirectX::XMFLOAT3 Direction = { -1.0f,-1.0f,-1.0f }, DirectX::XMFLOAT4 Position = { 0.0f,0.0f,0.0f,1.0f }, float Range = 10.0f, float CosTheta = 45.0f, const std::string& LightName = "SpotLight", int ShadowSize = 1024, ShadowTechnology Technology = ShadowTechnology::StandardShadowMap);
    static PointLight* AddPointLight(const std::string& LightName = "PointLight", DirectX::XMFLOAT4 Strength = { 1.0f,1.0f,1.0f,1.0f }, DirectX::XMFLOAT4 Position = { 0.0f,0.0f,0.0f,1.0f }, float Radius = 10.0f, int ShadowSize = 1024, ShadowTechnology Technology = ShadowTechnology::StandardShadowMap);

    static const std::vector<std::unique_ptr<DirectionLight>>& GetSceneDirectionalLights() { return m_SceneDirectionLights; }
    static const std::vector<std::unique_ptr<PointLight>>& GetScenePointLights() { return m_ScenePointLights; }
    static const std::vector<std::unique_ptr<SpotLight>>& GetSceneSpotLights() { return m_SceneSpotLights; }

    static const int GetNumLightsInScene() { return m_SceneDirectionLights.size() + m_SceneSpotLights.size() + m_ScenePointLights.size(); }
    /**
     * Get all light's light constants in the scene.
     * This function should only be called by shadowpass class.
     */
    const std::vector<LightConstants> GetSceneLightConstants()const;

protected:
    friend class Model;

    static Scene* ms_pScene;
private:
    friend CommandList;
    friend Model;

    struct RenderAABBCb
    {
        DirectX::XMFLOAT3 Extents;
        FLOAT             Padding1;
        DirectX::XMFLOAT4X4 ViewProj;
    };

    Scene();
    ~Scene();

    //For now,we do not support loading the same name model in one scene.
    static std::unordered_map<std::string, std::unique_ptr<Model>> m_SceneModelsMap;
    //A map for three types light.
    //Key:light type
    //Value:light vector
    static std::vector<std::unique_ptr<DirectionLight>> m_SceneDirectionLights;
    static std::vector<std::unique_ptr<SpotLight>> m_SceneSpotLights;
    static std::vector<std::unique_ptr<PointLight>> m_ScenePointLights;
    //For rendering aabb
    std::unique_ptr<RootSignature> m_pRenderAABBRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12RenderAABBPipelineState;
    Microsoft::WRL::ComPtr<ID3DBlob> m_AABBVS;
    Microsoft::WRL::ComPtr<ID3DBlob> m_AABBGS;
    Microsoft::WRL::ComPtr<ID3DBlob> m_AABBPS;
    //For frustum culling
    std::unique_ptr<FrustumCullinger> m_pFrustumCullinger;
    //
    DirectX::BoundingBox m_SceneBoundingBox;
    bool m_IsDirtyScene;
};