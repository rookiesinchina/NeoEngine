#pragma once

#include "Pass.h"





/************************************************************************/
/*A class is for rendering pass                                         */
/************************************************************************/
struct PassPipelineState
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

enum class ForwardPassType
{
    OpaquePass,
    TransparentPass
};

class ForwardRendering : public PassBase
{
public:
    ForwardRendering(ForwardPassType Type, std::shared_ptr<RenderTarget> pRenderTarget, const Camera* pOutputCamera);
    virtual ~ForwardRendering() {};
    /**
     * Execute this pass.If you do not set special pipeline or root signature.The @param:SetResourceFunc() should be empty,
     * which indicates that this function will use default method to render.However, if you set something,you should input
     * SetResourceFunc() and this function will contain some operation for each model,such as set vertex / index buffer,
     * set constant buffer,texture buffer,set primitive type,draw vertice and so on...
     */
    virtual void ExecutePass(
        std::shared_ptr<CommandList> commandList,
        std::function<void()> SetResourceFunc = {})override;
    /**
     * Update something about rendering.In most cases,the default is enough.But if you want to
     * update something you wish,you can use the second parameter.
     */
    virtual void UpdatePass(const UpdateEventArgs& Args, std::function<void()> UpdateFunc = {})override;

    void SetShadowState(bool State) { m_pForwardShdaowPass->SetShadowPassState(State); };
private:
    enum RenderingRootParameter
    {
        MeshConstantCB,                         //a cbv for mesh constant
        PassConstantCB,                         //a cbv for pass constant
        StructuredLight,                        //a srv for light constant. 
        StructuredMaterials,                    //a srv for structured materials.
        DiffuseTexture,                         //a table for diffuse textures,note this is only be saw at pixel shader.
        SpecularTexture,                        //a table for specular textures,note this is only be saw at pixel shader.
        HeightTexture,                          //a table for height textures,note this is only be saw at pixel shader.
        NormalTexture,                          //a table for normal textures,note this is only be saw at pixel shader.
        AmbientTexture,                         //a table for ambient textures,note this is only be saw at pixel shader.
        OpacityTexture,                         //a table for opacity textures,note this is only be saw at pixel shader.
        EmissiveTexture,                        //a table for emissive textures,note this is only be saw at pixel shader.

        DirectionAndSoptLightShadowTexture,     //a table for directional and spot light shadow
        PointLightShadowTexture,                //a table for point light shadow.
        NumRootParameters
    };

    struct ForwardRenderingPassConstants
    {
        DirectX::XMFLOAT4X4 View;
        DirectX::XMFLOAT4X4 InvView;
        DirectX::XMFLOAT4X4 Proj;
        DirectX::XMFLOAT4X4 InvProj;
        DirectX::XMFLOAT4X4 ViewProj;
        DirectX::XMFLOAT4X4 InvViewProj;

        DirectX::XMFLOAT3 EyePosW;
        INT               NumLights;
        DirectX::XMFLOAT2 RenderTargetSize;
        DirectX::XMFLOAT2 InvRenderTargetSize;
        FLOAT             ZNear;
        FLOAT             ZFar;
        FLOAT             DeltaTime;
        FLOAT             TotalTime;
        DirectX::XMFLOAT4 AmbientLight = { 0.3f,0.3f,0.3f,1.0f };
    };
private:
    ForwardRenderingPassConstants m_ForwardPassConstants;
    ForwardPassType m_ForwardType;

    std::unique_ptr<ShadowPass> m_pForwardShdaowPass;
};
