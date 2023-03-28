#pragma once

#include <memory>
#include <wrl.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <functional>

#include "Events.h"
#include "Light.h"
#include "DescriptorAllocation.h"

//@brief:pass class is a rendering pass class which is responsible for managing rendering pass.
//Such as shadow pass, transparent pass and so on.

const UINT m_MaxTextureNum = 128;
const UINT m_MaxDirectionAndSpotLightShadowNum = 5;
const UINT m_MaxPointLightShadowNum = 5;

class Texture;
class RootSignature;
class CommandList;
class Camera;
class FrustumCullinger;
class RenderTarget;


/************************************************************************/
/* A class is for shadow pass                                           */
/************************************************************************/
class ShadowPass
{
public:
    ShadowPass();
    virtual ~ShadowPass() {};

    void ExecutePass(std::shared_ptr<CommandList> commandList);

    const std::vector<LightConstants> GetLightConstants()const;
    /**
     * Get a special type light's shadow.If this light has no shadow or shadow pass state is off,then will return empty vector.
     */
    std::vector<const Texture*> GetDirectionAndSpotShadows()const;
    std::vector<const Texture*> GetPointShadows()const;

    void SetShadowPassState(bool State) { m_ShadowPassState = State; }

    const DescriptorAllocation& GetDirectionAndSpotDefaultSrvDescriptors()const { return m_DirectionalAndSpotDefaultSrv; }
    const DescriptorAllocation& GetPointDefaultSrvDescriptors()const { return m_PointDefaultSrv; }
private:
    /**
     * Get a special type light's shadow.If this light has no shadow or shadow pass state is off,then will return empty vector.
     */
    std::vector<const Texture*> GetShadows(LightType Type)const;

    bool m_ShadowPassState;

    DescriptorAllocation m_DirectionalAndSpotDefaultSrv;
    DescriptorAllocation m_PointDefaultSrv;
};

/************************************************************************/
/*A general class for all rendering                                     */
/************************************************************************/

class PassBase
{
public:
    PassBase(std::shared_ptr<RenderTarget> pRenderTarget,const Camera* pOutputCamera);
    virtual ~PassBase() {};

    void SetPassInput(const std::vector<const Model*>& pInputModels);
    void SetPassInput(const Model* pInputModel);

    void SetRenderTargets(std::shared_ptr<RenderTarget> pRenderTarget);
    /**
     * Set a specific pipeline state.
     * Note:This function should only be called when you need to set a special pipeline state.
     * If you do not need to set,DO NOT use this function!
     */
    void SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState);
    /**
     * Set a specific root signature.
     * Note:This function should only be called when you need to set a special root signature.
     * If you do not need to set,DO NOT use this function!
     */
    void SetRootSignature(std::shared_ptr<RootSignature> pRootSignature);

    virtual void ExecutePass(
        std::shared_ptr<CommandList> commandList,
        std::function<void()> SetResourceFunc = {});

    virtual void UpdatePass(const UpdateEventArgs& Args, std::function<void()> UpdateFunc = {}) = 0;

    const std::vector<const Model*>& GetInputModels()const { return m_pInputMoedels; }
protected:
    std::shared_ptr<RenderTarget> m_pRenderTarget;

    std::shared_ptr<RootSignature> m_pRootSignature;

    std::vector<const Model*> m_pInputMoedels;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_id3d12PassPipelineState;
    //
    std::unique_ptr<FrustumCullinger> m_pPassFrustumCullinger;
    //
    const Camera* m_pRenderingCamera;
};

