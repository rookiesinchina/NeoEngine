#include "Pass.h"
#include "Application.h"
#include "RootSignature.h"
#include "CommandList.h"
#include "Camera.h"
#include "FrustumCulling.h"
#include "DynamicDescriptorHeap.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/
ShadowPass::ShadowPass()
    :m_ShadowPassState(TRUE)
{
    //Since in most cases,we will not use bind all space in shader.So we need to  create default srv for shadow maps
    m_DirectionalAndSpotDefaultSrv = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_MaxDirectionAndSpotLightShadowNum);
    for (int i = 0; i < m_MaxDirectionAndSpotLightShadowNum; ++i)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC DirectionAndSpotSrv = {};
        DirectionAndSpotSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        DirectionAndSpotSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        DirectionAndSpotSrv.Texture2DArray.ArraySize = 1;
        DirectionAndSpotSrv.Texture2DArray.FirstArraySlice = 0;
        DirectionAndSpotSrv.Texture2DArray.MipLevels = 1;
        DirectionAndSpotSrv.Texture2DArray.MostDetailedMip = 0;
        DirectionAndSpotSrv.Texture2DArray.PlaneSlice = 0;
        DirectionAndSpotSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

        Application::GetApp()->GetDevice()->CreateShaderResourceView(nullptr, &DirectionAndSpotSrv, m_DirectionalAndSpotDefaultSrv.GetDescriptorHandle(i));
    }
    m_PointDefaultSrv = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_MaxPointLightShadowNum);
    for (int i = 0; i < m_MaxPointLightShadowNum; ++i)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC PointSrv = {};
        PointSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        PointSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        PointSrv.TextureCube.MipLevels = 1;
        PointSrv.TextureCube.MostDetailedMip = 0;
        PointSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

        Application::GetApp()->GetDevice()->CreateShaderResourceView(nullptr, &PointSrv, m_PointDefaultSrv.GetDescriptorHandle(i));
    }
};

void ShadowPass::ExecutePass(std::shared_ptr<CommandList> commandList)
{
    //If shadow pass state is on
    if (m_ShadowPassState)
    {
        for (const auto& directionlight : Scene::GetScene()->GetSceneDirectionalLights())
        {
            if (directionlight->GetRenderingShadowState())
            {
                directionlight->SetSceneBoundingBox(Scene::GetScene()->GetSceneBoundingBox());
                directionlight->RenderShadow(commandList);
            }
        }
        for (const auto& spotlight : Scene::GetScene()->GetSceneSpotLights())
        {
            if (spotlight->GetRenderingShadowState())
            {
                spotlight->RenderShadow(commandList);
            }
        }
        for (const auto& pointlight : Scene::GetScene()->GetScenePointLights())
        {
            if (pointlight->GetRenderingShadowState())
            {
                pointlight->RenderShadow(commandList);
            }
        }
    }
}

std::vector<const Texture*> ShadowPass::GetShadows(LightType Type)const
{
    std::vector<const Texture*> shadows;
    if (m_ShadowPassState)
    {
        switch (Type)
        {
        case Directional:
            for (const auto& directionlight : Scene::GetScene()->GetSceneDirectionalLights())
            {
                if (directionlight->GetRenderingShadowState())
                {
                    shadows.push_back(directionlight->GetLightShadow());
                }
            }
            break;
        case Spot:
            for (const auto& spotlight : Scene::GetScene()->GetSceneSpotLights())
            {
                if (spotlight->GetRenderingShadowState())
                {
                    shadows.push_back(spotlight->GetLightShadow());
                }
            }
            break;
        case Point:
            for (const auto& pointlight : Scene::GetScene()->GetScenePointLights())
            {
                if (pointlight->GetRenderingShadowState())
                {
                    shadows.push_back(pointlight->GetLightShadow());
                }
            }
            break;
        }
    }
    return shadows;
}

const std::vector<LightConstants> ShadowPass::GetLightConstants()const
{
    auto constants = Scene::GetScene()->GetSceneLightConstants();
    //If the shadow state is on,we need to set shadow index in light.
    if (m_ShadowPassState)
    {
        //since the sequence of lightconstant is direction -> sopt -> point.
        //so we can start from direction light,and spot light index is following with diectional light.
        //But point light has independent index from 0.
        UINT index = 0;
        UINT pos = 0;
        //for loop all directional lights
        for (const auto& directionlight : Scene::GetScene()->GetSceneDirectionalLights())
        {
            //if the light shadow state is on,then we think this light has a shadow map.
            if (directionlight->GetRenderingShadowState())
            {
                constants[pos].ShadowIndex = index;
                ++index;
            }
            ++pos;
        }
        for (const auto& spotlight : Scene::GetScene()->GetSceneSpotLights())
        {
            if (spotlight->GetRenderingShadowState())
            {
                constants[pos].ShadowIndex = index;
                ++index;
            }
            ++pos;
        }
        assert(index < m_MaxDirectionAndSpotLightShadowNum && "Error!Directional lights or spot lights have too many shadows!");
        //Note:for point lights,we need to index from 0.
        index = 0;
        for (const auto& pointlight : Scene::GetScene()->GetScenePointLights())
        {
            if (pointlight->GetRenderingShadowState())
            {
                constants[pos].ShadowIndex = index;
                ++index;
            }
            ++pos;
        }
        assert(index < m_MaxPointLightShadowNum && "Error!Point lights have too many shadows!");
    }
    return constants;
}

std::vector<const Texture*> ShadowPass::GetDirectionAndSpotShadows()const
{
    std::vector<const Texture*> shadows;
    auto directionalshadows = GetShadows(LightType::Directional);
    auto spotshadows = GetShadows(LightType::Spot);

    shadows.insert(shadows.end(), directionalshadows.begin(), directionalshadows.end());
    shadows.insert(shadows.end(), spotshadows.begin(), spotshadows.end());
    return shadows;
}

std::vector<const Texture*> ShadowPass::GetPointShadows()const
{
    return GetShadows(LightType::Point);
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
PassBase::PassBase(std::shared_ptr<RenderTarget> pRenderTarget,const Camera* pOutputCamera)
    :m_pRenderTarget(pRenderTarget)
    , m_pRootSignature(nullptr)
    , m_id3d12PassPipelineState(nullptr)
    , m_pPassFrustumCullinger(std::make_unique<FrustumCullinger>())
    , m_pRenderingCamera(pOutputCamera)
{};

void PassBase::SetPassInput(const std::vector<const Model*>& pInputModels)
{
    assert(pInputModels.size() && "Error! Input is null!");
    m_pInputMoedels = pInputModels;
}

void PassBase::SetPassInput(const Model* pInputModel)
{
    std::vector<const Model*> models = { pInputModel };
    SetPassInput(models);
}

void PassBase::SetRenderTargets(std::shared_ptr<RenderTarget> pRenderTarget)
{
    assert(pRenderTarget && "Error! Render target can not be null!");
    m_pRenderTarget = pRenderTarget;
}

void PassBase::SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState)
{
    assert(PipelineState && "Error! Pipeline state can not be null!");
    m_id3d12PassPipelineState = PipelineState;
}

void PassBase::SetRootSignature(std::shared_ptr<RootSignature> pRootSignature)
{
    assert(pRootSignature && "Error! Root signature can not be null!");
    m_pRootSignature = pRootSignature;
}

void PassBase::ExecutePass( 
    std::shared_ptr<CommandList> commandList, 
    std::function<void()> SetResourceFunc /* =  */ )
{
    assert(m_pRenderTarget && m_pRootSignature && "Error!This pass has not been set render target or root signature!");
    m_pPassFrustumCullinger->BindFrustumCamera(m_pRenderingCamera);

    CD3DX12_VIEWPORT ViewPort = CD3DX12_VIEWPORT(m_pRenderTarget->GetTexture(AttachmentPoint::Color0).GetD3D12Resource().Get());
    RECT ScissorRect = { 0,0,(int)ViewPort.Width,(int)ViewPort.Height };
    //Firstly,we need to clear and set render targets 
    {
        commandList->ClearRenderTarget(m_pRenderTarget.get());
        commandList->SetD3D12ViewPort(&ViewPort);
        commandList->SetD3D12ScissorRect(&ScissorRect);
        commandList->SetRenderTargets(*m_pRenderTarget);
    }
    commandList->SetD3D12PipelineState(m_id3d12PassPipelineState);
    commandList->SetGraphicsRootSignature(m_pRootSignature.get());

    //
    if (SetResourceFunc)
    {
        SetResourceFunc();
    }
}


