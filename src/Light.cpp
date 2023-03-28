#include "Light.h"
#include "Camera.h"
#include "Application.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include <array>

using namespace DirectX;
Light::Light(LightType Type, bool IsRenderShadow)
    : m_LightType(Type)
    , m_bRenderingShadow(IsRenderShadow)
{
    //here we create visualize light frustum root signature and pipeline state for debug
#ifdef _DEBUG
    //Create frustum vertex info
    m_LightVertex.resize(8);
    m_LightVertex[0].Pos = { -1.0f, +1.0f, 0.0f }; //
    m_LightVertex[1].Pos = { +1.0f, +1.0f, 0.0f }; // 1-----2
    m_LightVertex[2].Pos = { +1.0f, -1.0f, 0.0f }, // |     |
    m_LightVertex[3].Pos = { -1.0f, -1.0f, 0.0f }; // 0-----3
    //-------------------------------              //   Near
    m_LightVertex[4].Pos = { -1.0f, +1.0f, 1.0f }; // 5-----6
    m_LightVertex[5].Pos = { +1.0f, +1.0f, 1.0f }; // |     |
    m_LightVertex[6].Pos = { +1.0f, -1.0f, 1.0f }; // 4-----7
    m_LightVertex[7].Pos = { -1.0f, -1.0f, 1.0f }; //   Far  

    m_LightIndex.resize(24);
    m_LightIndex[0] = 0;
    m_LightIndex[1] = 1;
    m_LightIndex[2] = 1;
    m_LightIndex[3] = 2;
    m_LightIndex[4] = 2;
    m_LightIndex[5] = 3;
    m_LightIndex[6] = 3;
    m_LightIndex[7] = 0;

    m_LightIndex[8] = 4;
    m_LightIndex[9] = 5;
    m_LightIndex[10] = 5;
    m_LightIndex[11] = 6;
    m_LightIndex[12] = 6;
    m_LightIndex[13] = 7;
    m_LightIndex[14] = 7;
    m_LightIndex[15] = 4;

    m_LightIndex[16] = 0;
    m_LightIndex[17] = 4;
    m_LightIndex[18] = 1;
    m_LightIndex[19] = 5;
    m_LightIndex[20] = 2;
    m_LightIndex[21] = 6;
    m_LightIndex[22] = 3;
    m_LightIndex[23] = 7;

    //
    D3D12_FEATURE_DATA_ROOT_SIGNATURE rootVersion = {};
    rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(Application::GetApp()->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootVersion, sizeof(rootVersion))))
    {
        rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootDesc = {};
    versionRootDesc.Init_1_1(2, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    m_debugRootSignature = std::make_unique<RootSignature>(versionRootDesc.Desc_1_1, rootVersion.HighestVersion);
    //
    Microsoft::WRL::ComPtr<ID3DBlob> vs = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\VisualizeFrustum.hlsl", nullptr, "VS", "vs_5_1");
    Microsoft::WRL::ComPtr<ID3DBlob> ps = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\VisualizeFrustum.hlsl", nullptr, "PS", "ps_5_1");

    struct PipelineState
    {
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendDesc;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL         DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargetFormats;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DepthStencilFormat;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
    }pipelinestate;
    CD3DX12_DEPTH_STENCIL_DESC depthState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depthState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    D3D12_RT_FORMAT_ARRAY RtArray = {};
    RtArray.NumRenderTargets = 1;
    RtArray.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    DXGI_SAMPLE_DESC sampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(RtArray.RTFormats[0], Application::GetApp()->m_MultiSampleCount);

    D3D12_INPUT_ELEMENT_DESC inputelement[] =
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };

    pipelinestate.BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipelinestate.DepthStencil = depthState;
    pipelinestate.DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipelinestate.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
    pipelinestate.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
    pipelinestate.RenderTargetFormats = RtArray;
    pipelinestate.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipelinestate.InputLayout = { inputelement,1 };
    pipelinestate.pRootSignature = m_debugRootSignature->GetRootSignature().Get();
    pipelinestate.SampleDesc = sampleDesc;
    pipelinestate.PrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = { sizeof(pipelinestate) ,&pipelinestate };

    ThrowIfFailed(Application::GetApp()->GetDevice()->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_debugPipelineState)));
#endif

};

void Light::RenderLightFrustum(std::shared_ptr<CommandList> commandList,const Camera* pCamera)
{
#ifdef _DEBUG
    commandList->SetDynamicVertexBuffer(0, m_LightVertex.size(), sizeof(LightVertex),m_LightVertex.data());
    commandList->SetDynamicIndexBuffer(m_LightIndex.size(), DXGI_FORMAT_R16_UINT, m_LightIndex.data());
    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    commandList->SetD3D12PipelineState(m_debugPipelineState);
    commandList->SetGraphicsRootSignature(m_debugRootSignature.get());

    if (m_LightType == Point)
    {
        for (int i = 0; i < 6; ++i)
        {
            LightFrustumConstants frustumcb;
            DirectX::XMStoreFloat4x4(&frustumcb.FrustumInvView, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(GetLightViewMatrix(i)), GetLightViewMatrix(i))));
            DirectX::XMStoreFloat4x4(&frustumcb.FrustumInvProj, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(GetLightProjMatrix()), GetLightProjMatrix())));
            CameraConstants cameracb;
            DirectX::XMStoreFloat4x4(&cameracb.CameraViewProj, DirectX::XMMatrixTranspose(pCamera->GetViewProj()));

            commandList->SetGraphicsDynamicConstantBuffer(0, frustumcb);
            commandList->SetGraphicsDynamicConstantBuffer(1, cameracb);

            commandList->DrawIndexed(m_LightIndex.size(), 1, 0, 0, 0);
        }
    }
    else
    {
        LightFrustumConstants frustumcb;
        DirectX::XMStoreFloat4x4(&frustumcb.FrustumInvView, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(GetLightViewMatrix()), GetLightViewMatrix())));
        DirectX::XMStoreFloat4x4(&frustumcb.FrustumInvProj, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(GetLightProjMatrix()), GetLightProjMatrix())));
        CameraConstants cameracb;
        DirectX::XMStoreFloat4x4(&cameracb.CameraViewProj, DirectX::XMMatrixTranspose(pCamera->GetViewProj()));

        commandList->SetGraphicsDynamicConstantBuffer(0, frustumcb);
        commandList->SetGraphicsDynamicConstantBuffer(1, cameracb);

        commandList->DrawIndexed(m_LightIndex.size(), 1, 0, 0, 0);
    }
#endif
}

void Light::RenderShadow(std::shared_ptr<CommandList> commandList)
{
    if (m_bRenderingShadow)
    {
        m_pShadow->BeginShadow(commandList);
    }
}

void Light::CreateShadowForLight(const Light* pLight,int ShadowSize ,ShadowTechnology Technology,const Camera* pMainCamera)
{
    //here we need to initialize different shadow type
    switch (Technology)
    {
    case StandardShadowMap:
        m_pShadow = std::make_unique<Shadow>(ShadowSize, ShadowSize, this, 
            static_cast<DXGI_FORMAT>(StandardShadowMapFormat::StandardHighPresionFormat));
        break;
    case CascadedShadowMap:
        m_pShadow = std::make_unique<CascadedShadow>(ShadowSize, ShadowSize, this, pMainCamera, 
            static_cast<DXGI_FORMAT>(CascadedShadowMapFormat::CascadedHighPrecisionFormat),
            CascadedShadow::CASCADED_LEVEL::CAS_LEVEL4,CascadedShadow::FIT_PROJECTION_TO_CASCADES::FIT_TO_CASCADE,
            CascadedShadow::FIT_TO_NEAR_FAR::FIT_TO_CASCADE_AABB);
        break;
    case VarianceShadowMap:
        m_pShadow = std::make_unique<VarianceShadow>(ShadowSize, ShadowSize, this, 
            static_cast<DXGI_FORMAT>(VarianceShadowMapFormat::VarianceHighPrecisionFormat));
        break;
    case SATVarianceShadowMapFP:
        m_pShadow = std::make_unique<SATVarianceShadow>(ShadowSize, ShadowSize, this, 
            static_cast<DXGI_FORMAT>(SATVarianceShadowMapFPFormat::SATVarianceHighPrecisionFpFormat),
            ShadowTechnology::SATVarianceShadowMapFP);
        break;
    case SATVarianceShadowMapINT:
        m_pShadow = std::make_unique<SATVarianceShadow>(ShadowSize, ShadowSize, this, 
            static_cast<DXGI_FORMAT>(SATVarianceShadowMapINTFormat::SATVarianceHightPrecisionIntFormat), 
            ShadowTechnology::SATVarianceShadowMapINT);
        break;
    case CascadedVarianceShadowMap:
        m_pShadow = std::make_unique<CascadedVarianceShadow>(ShadowSize, ShadowSize, this, pMainCamera,
            static_cast<DXGI_FORMAT>(CascadedVarianceShadowMapFormat::CascadedVarianceHighPrecisionFormatINT), 
            CascadedVarianceShadow::CASCADED_LEVEL::CAS_LEVEL4, CascadedVarianceShadow::FIT_PROJECTION_TO_CASCADES::FIT_TO_CASCADE,
            CascadedVarianceShadow::FIT_TO_NEAR_FAR::FIT_TO_CASCADE_AABB);
        break;
    case NoShadow:
        m_pShadow = nullptr;
        break;
    }
}


//---------------------------------------------------------------------------------------------------------------------
//For directional light
DirectionLight::DirectionLight(DirectX::XMFLOAT4 Strength, DirectX::XMFLOAT3 Direction, const std::string& LightName /* = "NoName" */,int ShadowSize,ShadowTechnology Technology,const Camera* pMainCamera)
    : Light(LightType::Directional)
    , m_lightname(LightName)
    , m_Strength(Strength)
    //, m_IsFitViewFrustum(IsFitViewFrustum)
    //, m_pMainCamera(nullptr)
{
    DirectX::XMStoreFloat3(&m_Direction, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&Direction)));
    m_pLightCamera = std::make_unique<Camera>(CameraStyle::Orthographic);
    m_IsViewDirty = true;
    m_IsProjDirty = true;

    CreateShadowForLight(this, ShadowSize, Technology, pMainCamera);
};

DirectionLight::~DirectionLight() {};

void DirectionLight::SetSceneBoundingBox(const DirectX::BoundingBox& SceneAABB)
{
    m_HasSetBoundingBox = true;
    m_IsViewDirty = true;
    m_IsProjDirty = true;
    m_SceneAABB = SceneAABB;
}

DirectX::XMMATRIX DirectionLight::GetLightViewMatrix(int index /* = 0 */)const
{
    assert(m_HasSetBoundingBox && "Before getting light view matrix,you need to set bounding box of scene firstly!");

    if (m_IsViewDirty)
    {
        DirectX::XMVECTOR LightPos = DirectX::XMVectorMultiplyAdd(
            -DirectX::XMLoadFloat3(&m_Direction),
            DirectX::XMVector3Length(DirectX::XMLoadFloat3(&m_SceneAABB.Extents)),
            DirectX::XMLoadFloat3(&m_SceneAABB.Center));

        XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR Direction = XMLoadFloat3(&m_Direction);
        XMVECTOR Epsilon = XMVectorSet(0.1f, 0.1f, 0.1f, 0.1f);

        if (XMVector3NearEqual(Up, Direction, Epsilon))
        {
            Up = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
        }
        if (XMVector3NearEqual(-Up, Direction, Epsilon))
        {
            Up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }

        m_pLightCamera->LookAt(LightPos, DirectX::XMVectorAdd(LightPos, Direction), Up);
        m_pLightCamera->UpdateViewMatrix();

        m_IsViewDirty = false;
    }

    return m_pLightCamera->GetView();
}

DirectX::XMMATRIX DirectionLight::GetLightProjMatrix()const
{
    assert(m_HasSetBoundingBox && "Before getting light proj matrix,you need to set bounding box of scene firstly!");
    if (m_IsProjDirty)
    {
        //If fit to secen aabb....
        //if (!m_IsFitViewFrustum)
        {
            //for computing directional orthographic matrix,we need to transform world aabb to light space.
            DirectX::BoundingBox SceneAABBView;
            m_SceneAABB.Transform(SceneAABBView, GetLightViewMatrix());
            //we need to compute min and max xyz of bounding box in light view space
            std::array<XMFLOAT3, BoundingBox::CORNER_COUNT> Corners;
            SceneAABBView.GetCorners(Corners.data());

            XMVECTOR max = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);
            XMVECTOR min = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);

            for (const auto& Corner : Corners)
            {
                max = XMVectorMax(max, XMLoadFloat3(&Corner));
                min = XMVectorMin(min, XMLoadFloat3(&Corner));
            }

            const static float Bias = 0.005f;

            m_pLightCamera->SetLens(XMVectorGetX(min), XMVectorGetX(max), XMVectorGetY(min), XMVectorGetY(max), XMVectorGetZ(min) - Bias <= 0.0f ? XMVectorGetZ(min) : XMVectorGetZ(min) - Bias, XMVectorGetZ(max) + Bias);

            m_IsProjDirty = false;
        }
        //else//If fit to view frustum
        //{
        //    assert(m_pMainCamera && "Fitting to view frustum must have view camera firstly!");
        //    auto cameraProjection = m_pMainCamera->GetProj();
        //    DirectX::XMVECTOR FrustumCornersInViewSpace[8];
        //    GetFrustumPointsViewSpace(cameraProjection, FrustumCornersInViewSpace);
        //    //then,we need to transform these points into light view space
        //    DirectX::XMMATRIX CameraViewToWorld = m_pMainCamera->GetInvView();
        //    DirectX::XMMATRIX WorldToLightView = m_pLightCamera->GetView();
        //    DirectX::XMMATRIX CameraViewToLightView = CameraViewToWorld * WorldToLightView;
        //    DirectX::XMVECTOR FrustumCornersInLightViewSpace[8];
        //    for (int i = 0; i < 8; ++i)
        //    {
        //        FrustumCornersInLightViewSpace[i] = DirectX::XMVector3Transform(FrustumCornersInViewSpace[i], CameraViewToLightView);
        //    }
        //    //then we compute min and max in corners
        //    DirectX::XMVECTOR Max = DirectX::XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);
        //    DirectX::XMVECTOR Min = DirectX::XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
        //    for (int i = 0; i < 8; ++i)
        //    {
        //        Max = DirectX::XMVectorMax(Max, FrustumCornersInLightViewSpace[i]);
        //        Min = DirectX::XMVectorMin(Min, FrustumCornersInLightViewSpace[i]);
        //    }
        //    //then,we use min ,max and scene AABB to compute near and far plane
        //    //Firstly,we need to transform the corners of AABB to light view space
        //    DirectX::XMFLOAT3 AABBCornersWorld[8];
        //    m_SceneAABB.GetCorners(AABBCornersWorld);
        //    DirectX::XMVECTOR AABBCornerLightView[8];
        //    for (int i = 0; i < 8; ++i)
        //    {
        //        AABBCornerLightView[i] = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&AABBCornersWorld[i]), m_pLightCamera->GetView());
        //    }
        //    //
        //    float ZNear, ZFar = 0.0f;
        //    ComputeNearAndFar(ZNear, ZFar, Min, Max, AABBCornerLightView);
        //    //finally,we use znear ,zfar to get proj matrix
        //    m_pLightCamera->SetLens(DirectX::XMVectorGetX(Min), DirectX::XMVectorGetX(Max), DirectX::XMVectorGetY(Min), DirectX::XMVectorGetY(Max), ZNear, ZFar);
        //    m_IsProjDirty = false;
        //}
    }
    return m_pLightCamera->GetProj();
}


void DirectionLight::AdjustIntensity(float scaling)
{
    MathHelper::Clamp(scaling, 0.0f, 1.0f);
    m_Intensity = scaling;
}

void DirectionLight::SetDirection(DirectX::XMFLOAT3 Direction)
{
    XMStoreFloat3(&m_Direction, XMVector3Normalize(XMLoadFloat3(&Direction)));
    m_IsViewDirty = true;
    m_IsProjDirty = true;
}

const LightConstants& DirectionLight::GetLightConstant()
{
    m_LightConstant.Direction = m_Direction;
    m_LightConstant.Strength = m_Strength;
    m_LightConstant.Intensity = m_Intensity;
    m_LightConstant.Enable = m_Enable;
    m_LightConstant.Selected = m_Selected;
    m_LightConstant.Type = 0; 
    m_LightConstant.ShadowIndex = -1; //Default is -1.If this light is shadow on and the shadow pass is set,this value will be changed.
    m_LightConstant.ShadowType = (int)m_pShadow->GetShadowTechnology();

    XMStoreFloat4x4(&m_LightConstant.LightView[0], XMMatrixTranspose(m_pLightCamera->GetView()));
    XMStoreFloat4x4(&m_LightConstant.LightProj, XMMatrixTranspose(m_pLightCamera->GetProj()));
    XMStoreFloat4x4(&m_LightConstant.LightViewProj[0], XMMatrixTranspose(m_pLightCamera->GetInvViewProj()));

    return m_LightConstant;
}

void DirectionLight::SetEnableState(bool state)
{
    m_Enable = state;
}

void DirectionLight::SetSeletedState(bool seleted)
{
    m_Selected = seleted;
}

void DirectionLight::GetFrustumPointsViewSpace(
    DirectX::XMMATRIX EyeProjection,
    DirectX::XMVECTOR* FrustumPointsViewSpace)const
{
    DirectX::BoundingFrustum FrustumClip;
    DirectX::BoundingFrustum::CreateFromMatrix(FrustumClip, EyeProjection);

    //Points Layout
    //    Near        Far
    //  0------1    4------5
    //  |      |    |      |  
    //  |      |    |      |
    //  2------3    6------7

    DirectX::XMVECTOR LeftTopSlope = { FrustumClip.LeftSlope,FrustumClip.TopSlope,1.0f,0.0f };
    DirectX::XMVECTOR RightTopSlope = { FrustumClip.RightSlope,FrustumClip.TopSlope,1.0f,0.0f };
    DirectX::XMVECTOR LeftBottomSlope = { FrustumClip.LeftSlope,FrustumClip.BottomSlope,1.0f,0.0f };
    DirectX::XMVECTOR RightBottomSlope = { FrustumClip.RightSlope,FrustumClip.BottomSlope,1.0f,0.0f };

    DirectX::XMVECTOR Near = DirectX::XMVectorReplicate(FrustumClip.Near);
    DirectX::XMVECTOR Far = DirectX::XMVectorReplicate(FrustumClip.Far);

    DirectX::XMVECTOR Points[8];
    //Near
    Points[0] = DirectX::XMVectorMultiply(LeftTopSlope, Near);
    Points[1] = DirectX::XMVectorMultiply(RightTopSlope, Near);
    Points[2] = DirectX::XMVectorMultiply(LeftBottomSlope, Near);
    Points[3] = DirectX::XMVectorMultiply(RightBottomSlope, Near);
    //Far
    Points[4] = DirectX::XMVectorMultiply(LeftTopSlope, Far);
    Points[5] = DirectX::XMVectorMultiply(RightTopSlope, Far);
    Points[6] = DirectX::XMVectorMultiply(LeftBottomSlope, Far);
    Points[7] = DirectX::XMVectorMultiply(RightBottomSlope, Far);

    for (int i = 0; i < 8; ++i)
    {
        FrustumPointsViewSpace[i] = DirectX::XMVectorAdd(
            DirectX::XMVector3Rotate(Points[i], DirectX::XMLoadFloat4(&FrustumClip.Orientation)),
            DirectX::XMLoadFloat3(&FrustumClip.Origin));
    }
}

void DirectionLight::ComputeNearAndFar(FLOAT& fNearPlane,
    FLOAT& fFarPlane,
    FXMVECTOR vLightCameraOrthographicMin,
    FXMVECTOR vLightCameraOrthographicMax,
    XMVECTOR* pvPointsInCameraView)const
{
    // Initialize the near and far planes
    fNearPlane = FLT_MAX;
    fFarPlane = -FLT_MAX;

    Triangle triangleList[16];
    INT iTriangleCnt = 1;

    triangleList[0].pt[0] = pvPointsInCameraView[0];
    triangleList[0].pt[1] = pvPointsInCameraView[1];
    triangleList[0].pt[2] = pvPointsInCameraView[2];
    triangleList[0].culled = false;

    // These are the indices used to tesselate an AABB into a list of triangles.
    static const INT iAABBTriIndexes[] =
    {
        0,1,2,  0,2,3,
        4,5,6,  4,6,7,
        0,4,7,  0,3,7,
        1,6,5,  1,2,6,
        0,1,4,  1,4,5,
        2,3,6,  3,6,7
    };

    INT iPointPassesCollision[3];

    // At a high level: 
    // 1. Iterate over all 12 triangles of the AABB.  
    // 2. Clip the triangles against each plane. Create new triangles as needed.
    // 3. Find the min and max z values as the near and far plane.

    //This is easier because the triangles are in camera spacing making the collisions tests simple comparisions.

    float fLightCameraOrthographicMinX = XMVectorGetX(vLightCameraOrthographicMin);
    float fLightCameraOrthographicMaxX = XMVectorGetX(vLightCameraOrthographicMax);
    float fLightCameraOrthographicMinY = XMVectorGetY(vLightCameraOrthographicMin);
    float fLightCameraOrthographicMaxY = XMVectorGetY(vLightCameraOrthographicMax);

    for (INT AABBTriIter = 0; AABBTriIter < 12; ++AABBTriIter)
    {

        triangleList[0].pt[0] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 0]];
        triangleList[0].pt[1] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 1]];
        triangleList[0].pt[2] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 2]];
        iTriangleCnt = 1;
        triangleList[0].culled = FALSE;

        // Clip each invidual triangle against the 4 frustums.  When ever a triangle is clipped into new triangles, 
        //add them to the list.
        for (INT frustumPlaneIter = 0; frustumPlaneIter < 4; ++frustumPlaneIter)
        {

            FLOAT fEdge;
            INT iComponent;

            if (frustumPlaneIter == 0)
            {
                fEdge = fLightCameraOrthographicMinX; // todo make float temp
                iComponent = 0;
            }
            else if (frustumPlaneIter == 1)
            {
                fEdge = fLightCameraOrthographicMaxX;
                iComponent = 0;
            }
            else if (frustumPlaneIter == 2)
            {
                fEdge = fLightCameraOrthographicMinY;
                iComponent = 1;
            }
            else
            {
                fEdge = fLightCameraOrthographicMaxY;
                iComponent = 1;
            }

            for (INT triIter = 0; triIter < iTriangleCnt; ++triIter)
            {
                // We don't delete triangles, so we skip those that have been culled.
                if (!triangleList[triIter].culled)
                {
                    INT iInsideVertCount = 0;
                    XMVECTOR tempOrder;
                    // Test against the correct frustum plane.
                    // This could be written more compactly, but it would be harder to understand.

                    if (frustumPlaneIter == 0)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (XMVectorGetX(triangleList[triIter].pt[triPtIter]) >
                                XMVectorGetX(vLightCameraOrthographicMin))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if (frustumPlaneIter == 1)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (XMVectorGetX(triangleList[triIter].pt[triPtIter]) <
                                XMVectorGetX(vLightCameraOrthographicMax))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if (frustumPlaneIter == 2)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (XMVectorGetY(triangleList[triIter].pt[triPtIter]) >
                                XMVectorGetY(vLightCameraOrthographicMin))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (XMVectorGetY(triangleList[triIter].pt[triPtIter]) <
                                XMVectorGetY(vLightCameraOrthographicMax))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }

                    // Move the points that pass the frustum test to the begining of the array.
                    if (iPointPassesCollision[1] && !iPointPassesCollision[0])
                    {
                        tempOrder = triangleList[triIter].pt[0];
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;
                        iPointPassesCollision[1] = FALSE;
                    }
                    if (iPointPassesCollision[2] && !iPointPassesCollision[1])
                    {
                        tempOrder = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter].pt[2];
                        triangleList[triIter].pt[2] = tempOrder;
                        iPointPassesCollision[1] = TRUE;
                        iPointPassesCollision[2] = FALSE;
                    }
                    if (iPointPassesCollision[1] && !iPointPassesCollision[0])
                    {
                        tempOrder = triangleList[triIter].pt[0];
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;
                        iPointPassesCollision[1] = FALSE;
                    }

                    if (iInsideVertCount == 0)
                    { // All points failed. We're done,  
                        triangleList[triIter].culled = true;
                    }
                    else if (iInsideVertCount == 1)
                    {// One point passed. Clip the triangle against the Frustum plane
                        triangleList[triIter].culled = false;

                        // 
                        XMVECTOR vVert0ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[0];
                        XMVECTOR vVert0ToVert2 = triangleList[triIter].pt[2] - triangleList[triIter].pt[0];

                        // Find the collision ratio.
                        FLOAT fHitPointTimeRatio = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[0], iComponent);
                        // Calculate the distance along the vector as ratio of the hit ratio to the component.
                        FLOAT fDistanceAlongVector01 = fHitPointTimeRatio / XMVectorGetByIndex(vVert0ToVert1, iComponent);
                        FLOAT fDistanceAlongVector02 = fHitPointTimeRatio / XMVectorGetByIndex(vVert0ToVert2, iComponent);
                        // Add the point plus a percentage of the vector.
                        vVert0ToVert1 *= fDistanceAlongVector01;
                        vVert0ToVert1 += triangleList[triIter].pt[0];
                        vVert0ToVert2 *= fDistanceAlongVector02;
                        vVert0ToVert2 += triangleList[triIter].pt[0];

                        triangleList[triIter].pt[1] = vVert0ToVert2;
                        triangleList[triIter].pt[2] = vVert0ToVert1;

                    }
                    else if (iInsideVertCount == 2)
                    { // 2 in  // tesselate into 2 triangles


                        // Copy the triangle\(if it exists) after the current triangle out of
                        // the way so we can override it with the new triangle we're inserting.
                        triangleList[iTriangleCnt] = triangleList[triIter + 1];

                        triangleList[triIter].culled = false;
                        triangleList[triIter + 1].culled = false;

                        // Get the vector from the outside point into the 2 inside points.
                        XMVECTOR vVert2ToVert0 = triangleList[triIter].pt[0] - triangleList[triIter].pt[2];
                        XMVECTOR vVert2ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[2];

                        // Get the hit point ratio.
                        FLOAT fHitPointTime_2_0 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
                        FLOAT fDistanceAlongVector_2_0 = fHitPointTime_2_0 / XMVectorGetByIndex(vVert2ToVert0, iComponent);
                        // Calcaulte the new vert by adding the percentage of the vector plus point 2.
                        vVert2ToVert0 *= fDistanceAlongVector_2_0;
                        vVert2ToVert0 += triangleList[triIter].pt[2];

                        // Add a new triangle.
                        triangleList[triIter + 1].pt[0] = triangleList[triIter].pt[0];
                        triangleList[triIter + 1].pt[1] = triangleList[triIter].pt[1];
                        triangleList[triIter + 1].pt[2] = vVert2ToVert0;

                        //Get the hit point ratio.
                        FLOAT fHitPointTime_2_1 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
                        FLOAT fDistanceAlongVector_2_1 = fHitPointTime_2_1 / XMVectorGetByIndex(vVert2ToVert1, iComponent);
                        vVert2ToVert1 *= fDistanceAlongVector_2_1;
                        vVert2ToVert1 += triangleList[triIter].pt[2];
                        triangleList[triIter].pt[0] = triangleList[triIter + 1].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter + 1].pt[2];
                        triangleList[triIter].pt[2] = vVert2ToVert1;
                        // Cncrement triangle count and skip the triangle we just inserted.
                        ++iTriangleCnt;
                        ++triIter;


                    }
                    else
                    { // all in
                        triangleList[triIter].culled = false;

                    }
                }// end if !culled loop            
            }
        }
        for (INT index = 0; index < iTriangleCnt; ++index)
        {
            if (!triangleList[index].culled)
            {
                // Set the near and far plan and the min and max z values respectivly.
                for (int vertind = 0; vertind < 3; ++vertind)
                {
                    float fTriangleCoordZ = XMVectorGetZ(triangleList[index].pt[vertind]);
                    if (fNearPlane > fTriangleCoordZ)
                    {
                        fNearPlane = fTriangleCoordZ;
                    }
                    if (fFarPlane < fTriangleCoordZ)
                    {
                        fFarPlane = fTriangleCoordZ;
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------
//For spot light
SpotLight::SpotLight(DirectX::XMFLOAT4 Strength, DirectX::XMFLOAT3 Direction,DirectX::XMFLOAT4 Position ,float Range , float CosTheta ,const std::string& LightName /* = "SpotLight" */,int ShadowSize /* = 1024 */,ShadowTechnology Technology /* = StandardShadowMap */)
    :Light(LightType::Spot)
    , m_Strength(Strength)
    , m_Position(Position)
    , m_Range(Range)
    , m_CosTheta(CosTheta)
    , m_lightname(LightName)
{
    XMStoreFloat3(&m_Direction, XMVector3Normalize(XMLoadFloat3(&Direction)));
    m_pLightCamera = std::make_unique<Camera>(CameraStyle::Perspective);
    m_IsViewDirty = true;
    m_IsProjDirty = true;

    //here we need to initialize different shadow type.
    CreateShadowForLight(this, ShadowSize, Technology);
}

SpotLight::~SpotLight() {};

DirectX::XMMATRIX SpotLight::GetLightViewMatrix(int index /* = 0 */)const
{
    if (m_IsViewDirty)
    {
        XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR Direction = XMLoadFloat3(&m_Direction);
        XMVECTOR Epsilon = XMVectorSet(0.1f, 0.1f, 0.1f, 0.1f);

        if (XMVector3NearEqual(Up, Direction, Epsilon))
        {
            Up = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
        }
        if (XMVector3NearEqual(-Up, Direction, Epsilon))
        {
            Up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }

        m_pLightCamera->LookAt(XMLoadFloat4(&m_Position), XMVectorAdd(XMLoadFloat4(&m_Position), Direction), Up);
        m_pLightCamera->UpdateViewMatrix();

        m_IsViewDirty = false;
    }

    return m_pLightCamera->GetView();
}

DirectX::XMMATRIX SpotLight::GetLightProjMatrix()const
{
    if (m_IsProjDirty)
    {
        m_pLightCamera->SetLens(m_CosTheta, 1.0f, 0.1f, m_Range);
        m_IsProjDirty = false;
    }
    return m_pLightCamera->GetProj();
}

void SpotLight::AdjustIntensity(float scaling)
{
    MathHelper::Clamp(scaling, 0.0f, 1.0f);
    m_Intensity = scaling;
}

void SpotLight::SetDirection(DirectX::XMFLOAT3 Direction)
{
    XMStoreFloat3(&m_Direction, XMVector3Normalize(XMLoadFloat3(&Direction)));
    m_IsViewDirty = true;
}

void SpotLight::SetRange(float Radius)
{
    if (Radius < 0.3f)
    {
        Radius = 0.3f;
    }
    m_Range = Radius;
    m_IsProjDirty = true;
}

void SpotLight::SetCosTheta(float CosTheta)
{
    //Clamp costheta from 5 degree to 70 degree
    MathHelper::Clamp(CosTheta, 0.342f, 0.996f);
    m_CosTheta = CosTheta;
    m_IsProjDirty = true;
}

const LightConstants& SpotLight::GetLightConstant()
{
    m_LightConstant.Strength = m_Strength;
    m_LightConstant.Direction = m_Direction;
    m_LightConstant.Position = m_Position;
    m_LightConstant.Range = m_Range;
    m_LightConstant.SpotLightAngle = m_CosTheta;
    m_LightConstant.Intensity = m_Intensity;
    m_LightConstant.Enable = m_Enable;
    m_LightConstant.Selected = m_Selected;
    m_LightConstant.Type = 1;
    m_LightConstant.ShadowIndex = -1; //Default is -1.If this light is shadow on and the shadow pass is set,this value will be changed.
    m_LightConstant.ShadowType = (int)m_pShadow->GetShadowTechnology();

    XMStoreFloat4x4(&m_LightConstant.LightView[0], XMMatrixTranspose(m_pLightCamera->GetView()));
    XMStoreFloat4x4(&m_LightConstant.LightProj, XMMatrixTranspose(m_pLightCamera->GetProj()));
    XMStoreFloat4x4(&m_LightConstant.LightViewProj[0], XMMatrixTranspose(m_pLightCamera->GetInvViewProj()));
    
    return m_LightConstant;
}
//--------------------------------------------------------------------------
//For point light
PointLight::PointLight(DirectX::XMFLOAT4 Strength, DirectX::XMFLOAT4 Position, float Radius, const std::string& LightName /* = "PointLight" */, int ShadowSize /* = 1024 */, ShadowTechnology Technology /* = StandardShadowMap */)
    :Light(LightType::Point)
    , m_Strength(Strength)
    , m_Position(Position)
    , m_Range(Radius)
    , m_lightname(LightName)
{
    for (int i = 0; i < 6; ++i)
    {
        m_pLightCameras[i] = std::make_unique<Camera>(CameraStyle::Perspective);
    }
    m_IsViewDirty = true;
    m_IsProjDirty = true;

    CreateShadowForLight(this, ShadowSize, Technology);
};

PointLight::~PointLight() {};

DirectX::XMMATRIX PointLight::GetLightViewMatrix(int index )const
{
    if (m_IsViewDirty)
    {
        XMVECTOR X = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        XMVECTOR Y = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR Z = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        //This sequence is very important,since we will use these to generate cubemap for point light shadow.
        m_pLightCameras[0]->LookAt(XMLoadFloat4(&m_Position), XMVectorAdd(XMLoadFloat4(&m_Position), X), Y); //+X
        m_pLightCameras[1]->LookAt(XMLoadFloat4(&m_Position), XMVectorAdd(XMLoadFloat4(&m_Position), -X), Y);//-X
        m_pLightCameras[2]->LookAt(XMLoadFloat4(&m_Position), XMVectorAdd(XMLoadFloat4(&m_Position), Y), X); //+Y
        m_pLightCameras[3]->LookAt(XMLoadFloat4(&m_Position), XMVectorAdd(XMLoadFloat4(&m_Position), -Y), X);//-Y
        m_pLightCameras[4]->LookAt(XMLoadFloat4(&m_Position), XMVectorAdd(XMLoadFloat4(&m_Position), Z), Y); //+Z
        m_pLightCameras[5]->LookAt(XMLoadFloat4(&m_Position), XMVectorAdd(XMLoadFloat4(&m_Position), -Z), Y);//-Z

        m_IsViewDirty = false;
    }

    return m_pLightCameras[index]->GetView();
}

DirectX::XMMATRIX PointLight::GetLightProjMatrix()const
{
    if (m_IsProjDirty)
    {
        //For point light,six cameras have same proj matrix.
        for (auto& camera : m_pLightCameras)
        {
            camera->SetLens(XM_PIDIV2, 1.0f, 0.1f, m_Range);
        }
        m_IsProjDirty = false;
    }

    return m_pLightCameras[0]->GetProj();
}

void PointLight::AdjustIntensity(float scaling)
{
    MathHelper::Clamp(scaling, 0.0f, 1.0f);
    m_Intensity = scaling;
}

void PointLight::SetRange(float Radius)
{
    if (Radius < 0.3f)
    {
        Radius = 0.3f;
    }
    m_Range = Radius;
    m_IsProjDirty = true;
}

const LightConstants& PointLight::GetLightConstant()
{
    m_LightConstant.Strength = m_Strength;
    m_LightConstant.Position = m_Position;
    m_LightConstant.Range = m_Range;
    m_LightConstant.Intensity = m_Intensity;
    m_LightConstant.Enable = m_Enable;
    m_LightConstant.Selected = m_Selected;
    m_LightConstant.Type = 2;
    m_LightConstant.ShadowIndex = -1; //Default is -1.If this light is shadow on and the shadow pass is set,this value will be changed.
    m_LightConstant.ShadowType = (int)m_pShadow->GetShadowTechnology();

    for (int i = 0; i < 6; ++i)
    {
        XMStoreFloat4x4(&m_LightConstant.LightView[i], XMMatrixTranspose(m_pLightCameras[i]->GetView()));
        XMStoreFloat4x4(&m_LightConstant.LightProj, XMMatrixTranspose(m_pLightCameras[i]->GetProj()));
        XMStoreFloat4x4(&m_LightConstant.LightViewProj[i], XMMatrixTranspose(m_pLightCameras[i]->GetViewProj()));
    }
    return m_LightConstant;
}