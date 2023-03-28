#include "Camera.h"
#include "Application.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "CommandList.h"

#include <DirectXColors.h>

Camera::Camera(CameraStyle Style /* = Perspective */,bool IsMainCamera /* = false */)
    : m_CameraStyle(Style)
{
    //For visualize camera frustum.
    m_CameraVertex.resize(8);
    m_CameraVertex[0].Pos = { -1.0f, +1.0f, 0.0f }; //
    m_CameraVertex[1].Pos = { +1.0f, +1.0f, 0.0f }; // 1-----2
    m_CameraVertex[2].Pos = { +1.0f, -1.0f, 0.0f }, // |     |
    m_CameraVertex[3].Pos = { -1.0f, -1.0f, 0.0f }; // 0-----3
    //-------------------------------              //   Near
    m_CameraVertex[4].Pos = { -1.0f, +1.0f, 1.0f }; // 5-----6
    m_CameraVertex[5].Pos = { +1.0f, +1.0f, 1.0f }; // |     |
    m_CameraVertex[6].Pos = { +1.0f, -1.0f, 1.0f }; // 4-----7
    m_CameraVertex[7].Pos = { -1.0f, -1.0f, 1.0f }; //   Far  

    m_CameraIndex.resize(24);
    m_CameraIndex[0] = 0;
    m_CameraIndex[1] = 1;
    m_CameraIndex[2] = 1;
    m_CameraIndex[3] = 2;
    m_CameraIndex[4] = 2;
    m_CameraIndex[5] = 3;
    m_CameraIndex[6] = 3;
    m_CameraIndex[7] = 0;

    m_CameraIndex[8] = 4;
    m_CameraIndex[9] = 5;
    m_CameraIndex[10] = 5;
    m_CameraIndex[11] = 6;
    m_CameraIndex[12] = 6;
    m_CameraIndex[13] = 7;
    m_CameraIndex[14] = 7;
    m_CameraIndex[15] = 4;

    m_CameraIndex[16] = 0;
    m_CameraIndex[17] = 4;
    m_CameraIndex[18] = 1;
    m_CameraIndex[19] = 5;
    m_CameraIndex[20] = 2;
    m_CameraIndex[21] = 6;
    m_CameraIndex[22] = 3;
    m_CameraIndex[23] = 7;

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
}

void Camera::RenderCameraFrustum(std::shared_ptr<CommandList> commandList, const Camera* pCamera)const
{
    commandList->SetDynamicVertexBuffer(0, m_CameraVertex.size(), sizeof(LightVertex), m_CameraVertex.data());
    commandList->SetDynamicIndexBuffer(m_CameraIndex.size(), DXGI_FORMAT_R16_UINT, m_CameraIndex.data());
    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    commandList->SetD3D12PipelineState(m_debugPipelineState);
    commandList->SetGraphicsRootSignature(m_debugRootSignature.get());


    CameraFrustumConstants frustumcb;
    DirectX::XMStoreFloat4x4(&frustumcb.FrustumInvView, DirectX::XMMatrixTranspose(GetInvView()));
    DirectX::XMStoreFloat4x4(&frustumcb.FrustumInvProj, DirectX::XMMatrixTranspose(GetInvProj()));
    Constants cameracb;
    DirectX::XMStoreFloat4x4(&cameracb.CameraViewProj, DirectX::XMMatrixTranspose(pCamera->GetViewProj()));

    commandList->SetGraphicsDynamicConstantBuffer(0, frustumcb);
    commandList->SetGraphicsDynamicConstantBuffer(1, cameracb);

    commandList->DrawIndexed(m_CameraIndex.size(), 1, 0, 0, 0);

    for (auto subcamera : m_SubCameras)
    {
        subcamera->RenderCameraFrustum(commandList, pCamera);
    }
}


void Camera::SetPosition(const DirectX::XMFLOAT3& _Pos)
{
    mPosition = _Pos;
    mViewDirty = true;

    //Update sub cameras
    for (auto subcamera : m_SubCameras)
    {
        subcamera->SetPosition(_Pos);
    }
}

void Camera::SetPosition(float x, float y, float z)
{
    mPosition = { x,y,z };
    mViewDirty = true;

    //Update sub cameras
    for (auto subcamera : m_SubCameras)
    {
        subcamera->SetPosition(x, y, z);
    }
}

DirectX::XMVECTOR Camera::GetPosition()const
{
    return XMLoadFloat3(&mPosition);
}

DirectX::XMFLOAT3 Camera::GetPosition3f()const
{
    return mPosition;
}

DirectX::XMVECTOR Camera::GetRight()const
{
    return XMLoadFloat3(&mRight);
}

DirectX::XMFLOAT3 Camera::GetRight3f()const
{
    return mRight;
}

DirectX::XMVECTOR Camera::GetUp()const
{
    return XMLoadFloat3(&mUp);
}

DirectX::XMFLOAT3 Camera::GetUp3f()const
{
    return mUp;
}

DirectX::XMVECTOR Camera::GetLook()const
{
    return XMLoadFloat3(&mLook);
}

DirectX::XMFLOAT3 Camera::GetLook3f()const
{
    return mLook;
}

float Camera::GetNearZ()const
{
    return mZNear;
}

float Camera::GetFarZ()const
{
    return mZFar;
}

inline float Camera::GetFovY()const
{
    assert(m_CameraStyle == Perspective && "Error! Orthographic Camera does not have Fov attribute!");
    return mFovY;
}

float Camera::GetFovX()const
{
    assert(m_CameraStyle == Perspective && "Error! Orthographic Camera does not have Fov attribute!");
    return atan2f(0.5f * mNearWindowWidth, mZNear) * 2;
}

float Camera::GetAspect()const
{
    return mAspect;
}

float Camera::GetNearWindowWidth()const
{
    return mNearWindowWidth;
}

float Camera::GetNearWindowHeight()const
{
    return mNearWindowHeight;
}

float Camera::GetFarWindowWidth()const
{
    return mZFar * mNearWindowWidth / mZNear;
}

float Camera::GetFarWindowHeight()const
{
    return mZFar * mNearWindowHeight / mZNear;
}

void Camera::SetLens(float FovY, float Aspect, float Znear, float Zfar)
{
    assert(m_CameraStyle == Perspective && "Camera style does not accord with funtion!");

    mFovY = FovY;
    mAspect = Aspect;
    mZNear = Znear;
    mZFar = Zfar;

    DirectX::XMStoreFloat4x4(&mProj, DirectX::XMMatrixPerspectiveFovLH(
        mFovY, mAspect, mZNear, mZFar));
    
    DirectX::BoundingFrustum::CreateFromMatrix(mCameraPerspectiveFrustum, GetProj());
}

void Camera::SetLens(float ViewLeft, float ViewRight,float ViewBottom, float ViewTop, float Znear, float Zfar)
{
    assert(m_CameraStyle == Orthographic && "Camera style does not accord with funtion!");

    mZNear = Znear;
    mZFar = Zfar;

    DirectX::XMStoreFloat4x4(&mProj, DirectX::XMMatrixOrthographicOffCenterLH(ViewLeft, ViewRight, ViewBottom, ViewTop, Znear, Zfar));
    DirectX::XMFLOAT3 pt[8] =
    {
        { ViewLeft,ViewBottom,Znear },
        { ViewLeft,ViewTop,Znear },
        { ViewRight,ViewBottom,Znear },
        { ViewRight,ViewTop,Znear },
        //
        { ViewLeft,ViewBottom,Zfar },
        { ViewLeft,ViewTop,Zfar },
        { ViewRight,ViewBottom,Zfar },
        { ViewRight,ViewTop,Zfar }
    };
    mCameraOrthographicFrustum.CreateFromPoints(mCameraOrthographicFrustum, _countof(pt), pt, sizeof(DirectX::XMFLOAT3));
}

void Camera::LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up)
{
    DirectX::XMStoreFloat3(&mPosition, pos);
    DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, pos)));
    DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3Normalize(up));
    DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&mUp), DirectX::XMLoadFloat3(&mLook)));
    DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&mLook), DirectX::XMLoadFloat3(&mRight)));
    mViewDirty = true;

    //Update sub cameras
    for (auto subcamera : m_SubCameras)
    {
        subcamera->LookAt(pos, target, up);
    }
}

void Camera::LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up)
{
    DirectX::XMVECTOR _pos = DirectX::XMLoadFloat3(&pos);
    DirectX::XMVECTOR _target = DirectX::XMLoadFloat3(&target);
    DirectX::XMVECTOR _up = DirectX::XMLoadFloat3(&up);
    LookAt(_pos, _target, _up);
}
 
DirectX::XMMATRIX Camera::GetView()const
{
    UpdateViewMatrix();
    return DirectX::XMLoadFloat4x4(&mView);
}

DirectX::XMMATRIX Camera::GetInvView()const
{
    return DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(GetView()), GetView());
}

DirectX::XMMATRIX Camera::GetProj()const
{
    //If this camera is main camera,we use scenceAABB to compute zfar.
    if (mIsMainCamera && m_CameraStyle == Perspective && Scene::GetSceneState())
    {
        auto sceneAABB = Scene::GetScene()->GetSceneBoundingBox();
        DirectX::XMFLOAT3 sceneAABBCorners[8];
        sceneAABB.GetCorners(sceneAABBCorners);
        //transform corners into camera view space
        DirectX::XMVECTOR CornersInCameraView[8];
        for (int i = 0; i < 8; ++i)
        {
            CornersInCameraView[i] = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&sceneAABBCorners[i]), GetView());
        }
        //compute  max
        DirectX::XMVECTOR Max = { -FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX };
        for (int i = 0; i < 8; ++i)
        {
            Max = DirectX::XMVectorMax(Max, CornersInCameraView[i]);
        }
        mZFar = DirectX::XMVectorGetZ(Max);
        if (mZFar < 0.0f)
        {
            mZFar = mZNear + 0.01f;
        }
        //Update proj
        DirectX::XMStoreFloat4x4(&mProj, DirectX::XMMatrixPerspectiveFovLH(
            mFovY, mAspect, mZNear, mZFar));


    }
    return DirectX::XMLoadFloat4x4(&mProj);
}

DirectX::XMMATRIX Camera::GetInvProj()const
{
    return DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(GetProj()), GetProj());
}

DirectX::XMMATRIX Camera::GetViewProj()const
{
    return GetView() * GetProj();
}

DirectX::XMMATRIX Camera::GetInvViewProj()const
{
    return DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(GetViewProj()), GetViewProj());
}

DirectX::XMFLOAT4X4 Camera::GetView4x4f()const
{
    return mView;
}

DirectX::XMFLOAT4X4 Camera::GetProj4X4f()const
{
    return mProj;
}

void Camera::Walk(float d)
{
    DirectX::XMVECTOR oldpos = GetPosition();
    DirectX::XMVECTOR look = GetLook();

    DirectX::XMVECTOR newpos = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorReplicate(d), look, oldpos);
    DirectX::XMStoreFloat3(&mPosition, newpos);

    mViewDirty = true;

    //Update sub cameras
    for (auto subcamera : m_SubCameras)
    {
        subcamera->Walk(d);
    }
}

void Camera::Strafe(float d)
{
    DirectX::XMVECTOR pos = GetPosition();
    DirectX::XMVECTOR right = GetRight();

    DirectX::XMVECTOR newpos = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorReplicate(d), right, pos);
    DirectX::XMStoreFloat3(&mPosition, newpos);
    mViewDirty = true;

    //Update sub cameras
    for (auto subcamera : m_SubCameras)
    {
        subcamera->Strafe(d);
    }

}

void Camera::Pitch(float angle)
{
    DirectX::XMVECTOR look = GetLook();
    DirectX::XMVECTOR right = GetRight();
    DirectX::XMVECTOR up = GetUp();

    look = DirectX::XMVector3Transform(look, DirectX::XMMatrixRotationAxis(right, angle));
    up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationAxis(right, angle));

    DirectX::XMStoreFloat3(&mLook, look);
    DirectX::XMStoreFloat3(&mUp, up);
    mViewDirty = true;

    //Update sub cameras
    for (auto subcamera : m_SubCameras)
    {
        subcamera->Pitch(angle);
    }
}

void Camera::Roll(float angle)
{
    DirectX::XMVECTOR look = GetLook();
    DirectX::XMVECTOR right = GetRight();
    DirectX::XMVECTOR up = GetUp();

    right = DirectX::XMVector3Transform(right, DirectX::XMMatrixRotationAxis(look, angle));
    up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationAxis(look, angle));

    DirectX::XMStoreFloat3(&mRight, right);
    DirectX::XMStoreFloat3(&mUp, up);
    mViewDirty = true;

    //Update sub cameras
    for (auto subcamera : m_SubCameras)
    {
        subcamera->Roll(angle);
    }
}

void Camera::Head(float angle)
{
    DirectX::XMVECTOR look = GetLook();
    DirectX::XMVECTOR up = GetUp();
    DirectX::XMVECTOR right = GetRight();

    look = DirectX::XMVector3Transform(look, DirectX::XMMatrixRotationY(angle));
    up = DirectX::XMVector3Transform(up, DirectX::XMMatrixRotationY(angle));
    right = DirectX::XMVector3Transform(right, DirectX::XMMatrixRotationY(angle));

    DirectX::XMStoreFloat3(&mLook, look);
    DirectX::XMStoreFloat3(&mUp, up);
    DirectX::XMStoreFloat3(&mRight, right);

    mViewDirty = true;
    //Update sub cameras
    for (auto subcamera : m_SubCameras)
    {
        subcamera->Head(angle);
    }
}

void Camera::UpdateViewMatrix()const
{
    if (mViewDirty)
    {
        DirectX::XMVECTOR pos = GetPosition();
        DirectX::XMVECTOR look = DirectX::XMVector3Normalize(GetLook());
        DirectX::XMVECTOR up = DirectX::XMVector3Normalize(GetUp());
        DirectX::XMVECTOR right = DirectX::XMVector3Cross(up, look);
        look = DirectX::XMVector3Cross(right, up);
        up = DirectX::XMVector3Cross(look, right);

        float x = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(pos, right));
        float y = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(pos, up));
        float z = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(pos, look));

        mView(0, 0) = mRight.x;
        mView(1, 0) = mRight.y;
        mView(2, 0) = mRight.z;
        mView(3, 0) = x;

        mView(0, 1) = mUp.x;
        mView(1, 1) = mUp.y;
        mView(2, 1) = mUp.z;
        mView(3, 1) = y;

        mView(0, 2) = mLook.x;
        mView(1, 2) = mLook.y;
        mView(2, 2) = mLook.z;
        mView(3, 2) = z;

        mViewDirty = false;
    }
}

void Camera::GetCameraFrustum(DirectX::BoundingBox& OrthographicFrustum)const
{
    assert(m_CameraStyle == Orthographic && "Camera style does not accord with function");
    OrthographicFrustum = mCameraOrthographicFrustum;
}

void Camera::GetCameraFrustum(DirectX::BoundingFrustum& PerspectiveFrustum)const
{
    assert(m_CameraStyle == Perspective && "Camera style does not accord with function");
    PerspectiveFrustum = mCameraPerspectiveFrustum;
} 

void Camera::SetSubCameras(Camera* pCameras, UINT NumCameras)
{
    UINT InputPos = m_SubCameras.size();
    for (UINT i = 0; i < NumCameras; ++i)
    {
        m_SubCameras.push_back(static_cast<Camera*>(pCameras + sizeof(Camera) * i));
    }
    //Set same attribute with main camera
    for (UINT pos = InputPos; pos < m_SubCameras.size(); ++pos)
    {
        m_SubCameras[pos]->LookAt(GetPosition(), DirectX::XMVectorAdd(GetPosition(), GetLook()), GetUp());
    }

}

//void Camera::CreateRenderTextures(CAMERA_RENDERING_FLAGS Flags,int width, int height)
//{
//    if (Flags != CAMERA_RENDERING_FLAG_NONE)
//    {
//        assert(width > 0 && width > 0 && "Error!Texture dimiension can not be less than 0");
//        return;
//    }
//    switch (m_RenderingFlags)
//    {
//    case CAMERA_RENDERING_FLAG_NONE:
//        break;
//    case CAMERA_RENDERING_FLAG_COLOR:
//    {
//        DXGI_SAMPLE_DESC sampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(m_ColorFormat, D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT);
//
//        D3D12_RESOURCE_DESC ColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_ColorFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE ColorClear = CD3DX12_CLEAR_VALUE(m_ColorFormat, DirectX::Colors::BlueViolet);
//        Texture color(&ColorDesc, &ColorClear, TextureUsage::RenderTargetTexture, L"ColorTexture");
//
//        D3D12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            DXGI_FORMAT_D24_UNORM_S8_UINT, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
//        D3D12_CLEAR_VALUE depthClear = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
//        Texture depth(&depthDesc, &depthClear, TextureUsage::Depth, L"DepthStencil");
//
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color0, color);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::DepthStencil, depth);
//    }
//        break;
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH:
//    {
//        DXGI_SAMPLE_DESC sampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(m_ColorFormat, D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT);
//        //
//        D3D12_RESOURCE_DESC ColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_ColorFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE ColorClear = CD3DX12_CLEAR_VALUE(m_ColorFormat, DirectX::Colors::BlueViolet);
//        Texture color(&ColorDesc, &ColorClear, TextureUsage::RenderTargetTexture, L"ColorTexture");
//        //here we change sample desc
//        D3D12_RESOURCE_DESC renderDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_DepthFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE renderDepthClear = CD3DX12_CLEAR_VALUE(m_DepthFormat, DirectX::Colors::White);
//        Texture renderDepth(&renderDepthDesc, &renderDepthClear, TextureUsage::RenderTargetTexture, L"DepthTexture");
//        //
//        D3D12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            DXGI_FORMAT_D24_UNORM_S8_UINT, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
//        D3D12_CLEAR_VALUE depthClear = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
//        Texture depth(&depthDesc, &depthClear, TextureUsage::Depth, L"DepthStencil");
//
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color0, color);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color1, renderDepth);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::DepthStencil, depth);
//        //
//        D3D_SHADER_MACRO macro[] =
//        {
//            "COLOR_DEPTH","1",
//            NULL,NULL
//        };
//        Microsoft::WRL::ComPtr<ID3DBlob> PS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\Default.hlsl", macro, "PS", "ps_5_1");
//        D3D12_RT_FORMAT_ARRAY RtArray = {};
//        RtArray.NumRenderTargets = 2;
//        RtArray.RTFormats[0] = m_ColorFormat;
//        RtArray.RTFormats[1] = m_DepthFormat;
//
//        auto pipelinestate = Scene::GetScene()->GetSceneDefaultPipelineState();
//        pipelinestate.PS = CD3DX12_SHADER_BYTECODE(PS.Get());
//        pipelinestate.RenderTargetFormats = RtArray;
//        Microsoft::WRL::ComPtr<ID3D12PipelineState> newPipelineState;
//        ThrowIfFailed(Application::GetApp()->GetDevice()->CreatePipelineState(&pipelinestate, IID_PPV_ARGS(&newPipelineState)));
//        
//    }
//        break;
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL:
//    {
//        DXGI_SAMPLE_DESC sampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(m_ColorFormat, D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT);
//        //back buffer desc
//        D3D12_RESOURCE_DESC ColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_ColorFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE ColorClear = CD3DX12_CLEAR_VALUE(m_ColorFormat, DirectX::Colors::BlueViolet);
//        Texture color(&ColorDesc, &ColorClear, TextureUsage::RenderTargetTexture, L"ColorTexture");
//        //rendering depth desc
//        D3D12_RESOURCE_DESC renderDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_DepthFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE renderDepthClear = CD3DX12_CLEAR_VALUE(m_DepthFormat, DirectX::Colors::White);
//        Texture renderDepth(&renderDepthDesc, &renderDepthClear, TextureUsage::RenderTargetTexture, L"DepthTexture");
//        //rendering normal desc
//        D3D12_RESOURCE_DESC NormalDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_NormalFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE NormalClear = CD3DX12_CLEAR_VALUE(m_NormalFormat, DirectX::Colors::Red);
//        Texture normal(&NormalDesc, &NormalClear, TextureUsage::RenderTargetTexture, L"NormalTexture");
//        //depth stencil desc
//        D3D12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            DXGI_FORMAT_D24_UNORM_S8_UINT, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
//        D3D12_CLEAR_VALUE depthClear = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
//        Texture depth(&depthDesc, &depthClear, TextureUsage::Depth, L"DepthStencil");
//        //
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color0, color);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color1, renderDepth);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color2, normal);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::DepthStencil, depth);
//    }
//        break;
//    case CAMERA_RENDERING_FLAG_DEPTH:
//    {
//        DXGI_SAMPLE_DESC sampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(m_ColorFormat, D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT);
//        //rendering depth desc
//        D3D12_RESOURCE_DESC renderDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_DepthFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE renderDepthClear = CD3DX12_CLEAR_VALUE(m_DepthFormat, DirectX::Colors::White);
//        Texture renderDepth(&renderDepthDesc, &renderDepthClear, TextureUsage::RenderTargetTexture, L"DepthTexture");
//        //depth stencil desc
//        D3D12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            DXGI_FORMAT_D24_UNORM_S8_UINT, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
//        D3D12_CLEAR_VALUE depthClear = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
//        Texture depth(&depthDesc, &depthClear, TextureUsage::Depth, L"DepthStencil");
//        //
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color0, renderDepth);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::DepthStencil, depth);
//    }
//        break;
//    case CAMERA_RENDERING_FLAG_DEPTH_NORMAL:
//    {
//        DXGI_SAMPLE_DESC sampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(m_ColorFormat, D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT);
//        //rendering depth desc
//        D3D12_RESOURCE_DESC renderDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_DepthFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE renderDepthClear = CD3DX12_CLEAR_VALUE(m_DepthFormat, DirectX::Colors::White);
//        Texture renderDepth(&renderDepthDesc, &renderDepthClear, TextureUsage::RenderTargetTexture, L"DepthTexture");
//        //rendering normal desc
//        D3D12_RESOURCE_DESC NormalDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_NormalFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE NormalClear = CD3DX12_CLEAR_VALUE(m_NormalFormat, DirectX::Colors::Red);
//        Texture normal(&NormalDesc, &NormalClear, TextureUsage::RenderTargetTexture, L"NormalTexture");
//        //depth stencil desc
//        D3D12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            DXGI_FORMAT_D24_UNORM_S8_UINT, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
//        D3D12_CLEAR_VALUE depthClear = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
//        Texture depth(&depthDesc, &depthClear, TextureUsage::Depth, L"DepthStencil");
//        //
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color0, renderDepth);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color1, normal);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::DepthStencil, depth);
//    }
//        break;
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL_SPECULAR:
//    {
//        //reserved for specular
//    }
//        break;
//    case CAMERA_RENDERING_FLAG_NORMAL:
//    {
//        DXGI_SAMPLE_DESC sampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(m_ColorFormat, D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT);
//        //rendering normal desc
//        D3D12_RESOURCE_DESC NormalDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            m_NormalFormat, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
//        D3D12_CLEAR_VALUE NormalClear = CD3DX12_CLEAR_VALUE(m_NormalFormat, DirectX::Colors::Red);
//        Texture normal(&NormalDesc, &NormalClear, TextureUsage::RenderTargetTexture, L"NormalTexture");
//        //depth stencil desc
//        D3D12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
//            DXGI_FORMAT_D24_UNORM_S8_UINT, width, height,
//            1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
//        D3D12_CLEAR_VALUE depthClear = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);
//        Texture depth(&depthDesc, &depthClear, TextureUsage::Depth, L"DepthStencil");
//        //
//        m_pRenderTarget->AttachTexture(AttachmentPoint::Color1, normal);
//        m_pRenderTarget->AttachTexture(AttachmentPoint::DepthStencil, depth);
//    }
//        break;
//    case CAMERA_RENDERING_FLAG_SPECULAR:
//    {
//        //reserved for specular
//    }
//        break;
//    case CAMERA_RENDERING_FLAG_ALL:
//    {
//        //reserved for specular
//    }
//        break;
//    default:
//    {
//        assert(FALSE && "Unexcepted Error!");
//    }
//        break;
//    }
//}
//

//
//void Camera::ResizeRenderingTextures(int newWidth, int newHeight)
//{
//    if (m_pRenderTarget)
//    {
//        m_pRenderTarget->Resize(newWidth, newHeight);
//    }
//}
//
//bool Camera::SupportDepthFormat(DXGI_FORMAT Format)
//{
//    switch (Format)
//    {
//    case DXGI_FORMAT_R16_FLOAT:         //for normal depth
//    case DXGI_FORMAT_R32_FLOAT:         //for normal depth
//    case DXGI_FORMAT_R16G16_FLOAT:      //for variance shadow map
//    case DXGI_FORMAT_R32G32_FLOAT:      //for variance shadow map
//    case DXGI_FORMAT_R16G16B16A16_FLOAT://for sat variance shadow map fp
//    case DXGI_FORMAT_R32G32B32A32_FLOAT://for sat variance shadow map fp
//    case DXGI_FORMAT_R16G16_UINT:       //for sat variance shadow map int
//    case DXGI_FORMAT_R32G32_UINT:       //for sat variance shadow map int
//        return true;
//    default:
//        return false;
//    }
//}
//
//bool Camera::SupportNormalFormat(DXGI_FORMAT Format)
//{
//    switch (Format)
//    {
//    case DXGI_FORMAT_R16G16B16A16_FLOAT:
//    case DXGI_FORMAT_R32G32B32A32_FLOAT:
//        return true;
//    default:
//        return false;
//    }
//}
//
//void Camera::SetDepthFormat(DXGI_FORMAT Format)
//{
//    if (SupportDepthFormat(Format))
//    {
//        switch (m_RenderingFlags)
//        {
//        case CAMERA_RENDERING_FLAG_NONE:
//        case CAMERA_RENDERING_FLAG_COLOR:
//        case CAMERA_RENDERING_FLAG_NORMAL:
//            //Do nothing.
//            break;
//        case CAMERA_RENDERING_FLAG_COLOR_DEPTH:
//        case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL:
//        case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL_SPECULAR:
//            m_pRenderTarget->SetFormat(AttachmentPoint::Color1, Format);
//            break;
//        case CAMERA_RENDERING_FLAG_DEPTH:
//        case CAMERA_RENDERING_FLAG_DEPTH_NORMAL:
//            m_pRenderTarget->SetFormat(AttachmentPoint::Color0, Format);
//            break;
//        case CAMERA_RENDERING_FLAG_SPECULAR:
//            //reserved for specular
//            break;
//        case CAMERA_RENDERING_FLAG_ALL:
//            //reserved for specular.
//            break;
//        }
//    }
//}
//
//const Texture& Camera::GetColorTexture()const
//{
//    switch (m_RenderingFlags)
//    {
//    case CAMERA_RENDERING_FLAG_NONE:
//    case CAMERA_RENDERING_FLAG_NORMAL:
//    case CAMERA_RENDERING_FLAG_DEPTH:
//    case CAMERA_RENDERING_FLAG_DEPTH_NORMAL:
//        return Texture();
//    case CAMERA_RENDERING_FLAG_COLOR:
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH:
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL:
//        return m_pRenderTarget->GetTexture(AttachmentPoint::Color0);
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL_SPECULAR://reserved for specular
//        break;
//    case CAMERA_RENDERING_FLAG_SPECULAR:
//        break;
//    case CAMERA_RENDERING_FLAG_ALL:
//        break;
//    }
//}
//
//const Texture& Camera::GetDepthTexture()const
//{
//    switch (m_RenderingFlags)
//    {
//    case CAMERA_RENDERING_FLAG_NONE:
//    case CAMERA_RENDERING_FLAG_COLOR:
//    case CAMERA_RENDERING_FLAG_NORMAL:
//        return Texture();
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH:
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL:
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL_SPECULAR:
//        return m_pRenderTarget->GetTexture(AttachmentPoint::Color1);
//    case CAMERA_RENDERING_FLAG_DEPTH:
//    case CAMERA_RENDERING_FLAG_DEPTH_NORMAL:
//        return m_pRenderTarget->GetTexture(AttachmentPoint::Color0);
//    case CAMERA_RENDERING_FLAG_SPECULAR:
//        break;
//    case CAMERA_RENDERING_FLAG_ALL:
//        break;
//    }
//}
//
//const Texture& Camera::GetNormalTexture()const
//{
//    switch (m_RenderingFlags)
//    {
//    case CAMERA_RENDERING_FLAG_NONE:
//    case CAMERA_RENDERING_FLAG_COLOR:
//    case CAMERA_RENDERING_FLAG_DEPTH:
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH:
//        return Texture();
//    case CAMERA_RENDERING_FLAG_NORMAL:
//        return m_pRenderTarget->GetTexture(AttachmentPoint::Color0);
//    case CAMERA_RENDERING_FLAG_DEPTH_NORMAL:
//        return m_pRenderTarget->GetTexture(AttachmentPoint::Color1);
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL:
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL_SPECULAR:
//        return m_pRenderTarget->GetTexture(AttachmentPoint::Color2);
//    case CAMERA_RENDERING_FLAG_SPECULAR:
//        break;
//    case CAMERA_RENDERING_FLAG_ALL:
//        break;
//    }
//}
//
//void Camera::RenderScene(D3D12_VIEWPORT ViewPort, RECT ScissorRect, std::shared_ptr<CommandList> commandList)
//{
//    switch (m_RenderingFlags)
//    {
//    case CAMERA_RENDERING_FLAG_NONE:
//        break;
//    case CAMERA_RENDERING_FLAG_COLOR:
//    case CAMERA_RENDERING_FLAG_DEPTH:
//    case CAMERA_RENDERING_FLAG_NORMAL:
//    case CAMERA_RENDERING_FLAG_SPECULAR:
//    {
//        //clear render target
//        {
//            commandList->ClearRenderTargetTexture(&m_pRenderTarget->GetTexture(AttachmentPoint::Color0), DirectX::Colors::White);
//            commandList->ClearDepthStencilTexture(&m_pRenderTarget->GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
//        }
//        //Set render target and depth stencil
//        {
//            commandList->SetD3D12ViewPort(&ViewPort);
//            commandList->SetD3D12ScissorRect(&ScissorRect);
//            commandList->SetRenderTargets(*m_pRenderTarget);
//        }
//        Scene::GetScene()->RenderScene(commandList);
//
//    }
//
//        break;
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH:
//    case CAMERA_RENDERING_FLAG_DEPTH_NORMAL:
//        break;
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL:
//        break;
//    case CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL_SPECULAR:
//        break;
//    case CAMERA_RENDERING_FLAG_ALL:
//        break;
//    }
//    //Clear render target
//    {
//        
//    }
//}