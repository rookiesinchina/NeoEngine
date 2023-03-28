#pragma once
#include "d3dUtil.h"
#include "MathHelper.h"
#include <memory>


//--------------------------------------
//Just for visualize camera frustum
struct CameraFrustumConstants
{
    DirectX::XMFLOAT4X4 FrustumInvView;
    DirectX::XMFLOAT4X4 FrustumInvProj;
};

struct Constants
{
    DirectX::XMFLOAT4X4 CameraViewProj;
};
//-------------------------------------------

enum CameraStyle
{
    Perspective,
    Orthographic,
};

typedef enum CAMERA_RENDERING_FLAGS
{
    CAMERA_RENDERING_FLAG_NONE,   
    CAMERA_RENDERING_FLAG_COLOR,   
    CAMERA_RENDERING_FLAG_COLOR_DEPTH,
    CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL,
    CAMERA_RENDERING_FLAG_DEPTH,
    CAMERA_RENDERING_FLAG_DEPTH_NORMAL,
    CAMERA_RENDERING_FLAG_COLOR_DEPTH_NORMAL_SPECULAR,
    CAMERA_RENDERING_FLAG_NORMAL,   
    CAMERA_RENDERING_FLAG_SPECULAR,
    CAMERA_RENDERING_FLAG_ALL
}CAMERA_RENDERING_FLAGS;

class Texture;
class RenderTarget;
class CommandList;
class RootSignature;

class Camera
{
private:
    mutable DirectX::XMFLOAT3 mPosition = { 0.0f,0.0f,0.0f };
    mutable DirectX::XMFLOAT3 mUp = { 0.0f,1.0f,0.0f };
    mutable DirectX::XMFLOAT3 mLook = { 0.0f,0.0f,1.0f };
    mutable DirectX::XMFLOAT3 mRight = { 1.0f,0.0f,0.0f };

    float mZNear = 0.0f;
    mutable float mZFar = 1.0f;
    float mFovY = 0.0f;
    float mAspect = 0.0f;
    float mNearWindowHeight = 0.0f;
    float mNearWindowWidth = 0.0f;

    mutable bool mViewDirty = true;
    bool mIsMainCamera = false;

    mutable DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
    mutable DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    //Frustum Bounding in camera coordinates
    //Note:For perspective camera,the bounding box is a frustum
    //and for orthographic camera,the bounding box is a aabb.
    DirectX::BoundingFrustum mCameraPerspectiveFrustum;
    DirectX::BoundingBox     mCameraOrthographicFrustum;
    //Type
    CameraStyle m_CameraStyle;
    //Sub cameras
    std::vector<Camera*> m_SubCameras;

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 Pos;
    };

    std::vector<Vertex> m_CameraVertex;
    std::vector<uint16_t> m_CameraIndex;
    std::unique_ptr<RootSignature> m_debugRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_debugPipelineState;

    //Rendering textures
    //CAMERA_RENDERING_FLAGS m_RenderingFlags;
    //std::unique_ptr<RenderTarget> m_pRenderTarget;

    //int m_Width;
    //int m_Height;

    //DXGI_FORMAT m_ColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    //DXGI_FORMAT m_DepthFormat = DXGI_FORMAT_R16_FLOAT;
    //DXGI_FORMAT m_NormalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    ////reserved for specular texture
    //DXGI_FORMAT m_SpecularFormat = DXGI_FORMAT_R16_FLOAT;


    //bool SupportDepthFormat(DXGI_FORMAT Format);
    //bool SupportNormalFormat(DXGI_FORMAT Format);
    //bool SupportSpecularFormat(DXGI_FORMAT Format);
public:
    /**
     * Default:camera is perspective camera and not main camera.
     */
    Camera(CameraStyle Style = Perspective,bool IsMainCamera = false);
    ~Camera() {};

    void SetPosition(const DirectX::XMFLOAT3& _Pos);
    void SetPosition(float x, float y, float z);
    DirectX::XMVECTOR GetPosition()const;
    DirectX::XMFLOAT3 GetPosition3f()const;

    DirectX::XMVECTOR GetRight()const;
    DirectX::XMFLOAT3 GetRight3f()const;
    DirectX::XMVECTOR GetUp()const;
    DirectX::XMFLOAT3 GetUp3f()const;
    DirectX::XMVECTOR GetLook()const;
    DirectX::XMFLOAT3 GetLook3f()const;

    float GetNearZ()const;
    float GetFarZ()const;
    float GetFovY()const;
    float GetFovX()const;
    float GetAspect()const;

    float GetNearWindowWidth()const;
    float GetNearWindowHeight()const;
    float GetFarWindowWidth()const;
    float GetFarWindowHeight()const;

    /**
     * Note:If this camera is main camera,then the zfar will be decided by scene AABB.so the @param:zfar can be set by random.
     * Note:The main camera can not be orthographic style.
     */
    void SetLens(float FovY, float Aspect, float Znear, float Zfar);
    void SetLens(float ViewLeft, float ViewRight, float ViewBottom, float ViewTop, float Znear, float Zfar);

    void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up);
    void LookAt(const DirectX::XMFLOAT3& pos,const DirectX::XMFLOAT3& target,const DirectX::XMFLOAT3& up);

    DirectX::XMMATRIX GetView()const;
    DirectX::XMMATRIX GetInvView()const;
    DirectX::XMMATRIX GetProj()const;
    DirectX::XMMATRIX GetInvProj()const;
    DirectX::XMMATRIX GetViewProj()const;
    DirectX::XMMATRIX GetInvViewProj()const;

    DirectX::XMFLOAT4X4 GetView4x4f()const;
    DirectX::XMFLOAT4X4 GetProj4X4f()const;

    void Walk(float d);
    void Strafe(float d);
    void Pitch(float angle);
    void Roll(float angle);
    void Head(float angle);

    void UpdateViewMatrix()const;

    void GetCameraFrustum(DirectX::BoundingFrustum& PerspectiveFrustum)const;
    void GetCameraFrustum(DirectX::BoundingBox& OrthographicFrustum)const;

    CameraStyle GetCameraStyle()const { return m_CameraStyle; }
    //Set a camera as main camera or not.
    //Note:If a camera is main camera,then the the zfar of this camera will always be the farthest distance of scene AABB in camera space.
    //So if a camera will be used for shadow,please do not set this camera is main camera.
    void SetMainCamera(bool MainCameraState) { mIsMainCamera = MainCameraState; }

    void RenderCameraFrustum(std::shared_ptr<CommandList> commandList, const Camera* pCamera)const;
    //Set a serial of sub cameras for this camera,sub cameras will have same attribute with main camera except projection matrix and type.
    void SetSubCameras(Camera* pCameras, UINT NumCameras);
    //------------------------------------------------------------------------------
    //Following function is responsible for rendering

    //Create rendering textures of this camera.
    //Note:This function must be called after all models have been added into scene!
    //void CreateRenderTextures(CAMERA_RENDERING_FLAGS Flags,int width, int height);
    ////Note:Resize all textures!
    //void ResizeRenderingTextures(int newWidth, int newHeight);

    //void SetDepthFormat(DXGI_FORMAT Format);
    ////Set a format for normal.If the format is not supported,will do nothing.
    //void SetNormalFormat(DXGI_FORMAT Format);

    //void SetSpecularFormat(DXGI_FORMAT Format);
    ////Get specific texture.If the texture which you want to get has not been created,then will return a invalid texture.
    //const Texture& GetColorTexture()const;
    //const Texture& GetDepthTexture()const;
    //const Texture& GetNormalTexture()const;
    //const Texture& GetSpecularTexture()const;

    //void UpdateScene();
    ////Rendering scene by using textures which you wish to rendering.
    //void RenderScene(D3D12_VIEWPORT ViewPort, RECT ScissorRect, std::shared_ptr<CommandList> commandList);

    //void RenderingEnvironment();
};
