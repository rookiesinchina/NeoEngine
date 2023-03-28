#pragma once
#include <DirectXMath.h>
#include <string>
#include <DirectXCollision.h>
#include <array>
#include <memory>

#include "MathHelper.h"
#include "MathHelper.h"
#include "Shadow.h"

#ifdef _DEBUG
struct LightFrustumConstants
{
    DirectX::XMFLOAT4X4 FrustumInvView;
    DirectX::XMFLOAT4X4 FrustumInvProj;
};

struct CameraConstants
{
    DirectX::XMFLOAT4X4 CameraViewProj;
};

struct LightVertex
{
    DirectX::XMFLOAT3 Pos;
};
#endif


struct LightConstants
{
    DirectX::XMFLOAT4 Strength = { 1.0f,1.0f,1.0f,1.0f };
    DirectX::XMFLOAT4 Position = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT3 Direction = { 1.0f,0.0f,0.0f };
    FLOAT SpotLightAngle = 45.0f;   //cone angle.
    //-----------------------------
    FLOAT Range = 5.0f;
    FLOAT Intensity = 1.0f;
    BOOL  Enable = TRUE;
    BOOL  Selected = FALSE;
    //
    DirectX::XMFLOAT4X4 LightView[6] = { MathHelper::Identity4x4() };       //For directional and spot light,we just use the first one matrix
    DirectX::XMFLOAT4X4 LightProj    =  MathHelper::Identity4x4() ;         //For point light , use six matrixs.
    DirectX::XMFLOAT4X4 LightViewProj[6] = { MathHelper::Identity4x4() };
    //------------------------------
    int  Type = 0;         //0: directional light 1: spot light 2: point light
    int  ShadowIndex = -1; //The default index -1 indicates that this light has no shadow.
    int  ShadowType = 0;   //Shadow type:
                           //1: standard shadow map 
                           //2: variance shadow map
                           //3: sat variance shadow map fp 
                           //4: sat variance shadow map int 
                           //5: cascaded shadow map
                           //6: cascaded variance shadow map
    int  Padding;
};


enum LightType
{
    Directional,
    Spot,
    Point
};

class Camera;
class Shadow;
class ShadowBase;

class Light
{
public:
    Light(LightType Type, bool IsRenderShadow = true);

    virtual ~Light() {};
    //Get light view matrix.
    //Note:For directional and spot light,DO NOT set index parameter!
    //This parameter is only for point light!
    virtual DirectX::XMMATRIX GetLightViewMatrix(int index = 0)const = 0;
    //Get light camera.
    //Note:For directional and spot light,DO NOT set index parameter!
    //This parameter is only for point light!
    virtual Camera* GetLightCamera(int index = 0)const = 0;

    virtual DirectX::XMMATRIX GetLightProjMatrix()const = 0;

    virtual void SetStrength(DirectX::XMFLOAT4 Strength) = 0;

    virtual void AdjustIntensity(float scaling) = 0;

    virtual void SetEnableState(bool state) = 0;

    virtual void SetSeletedState(bool seleted) = 0;

    virtual const LightConstants& GetLightConstant() = 0;

    virtual void ResizeShadowMap(int newWidth, int newHeight) = 0;

    virtual void SetShadowTechnology(ShadowTechnology Technology) = 0;

    virtual void SetShadowFormat(DXGI_FORMAT Format) = 0;

    void RenderLightFrustum(std::shared_ptr<CommandList> commandList,const Camera* pCamera);
    //-----------------------------------------------------------------------------------------------
    LightType GetLightType()const { return m_LightType; }

    void SetRenderingShadowState(bool IsRenderingShadow) { m_bRenderingShadow = IsRenderingShadow; }

    void RenderShadow(std::shared_ptr<CommandList> commandList);

    void CreateShadowForLight(const Light* pLight, int ShadowSize, ShadowTechnology Technology,const Camera* pMainCamera = nullptr);

    bool GetRenderingShadowState()const { return m_bRenderingShadow; }

    const Texture* GetLightShadow()const { return m_pShadow->GetShadow(); }
protected:
    LightType m_LightType;
    bool m_bRenderingShadow;

    std::unique_ptr<ShadowBase> m_pShadow;

#ifdef _DEBUG
    std::unique_ptr<RootSignature> m_debugRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_debugPipelineState;
    std::vector<LightVertex> m_LightVertex;
    std::vector<uint16_t>    m_LightIndex;
#endif
};

/************************************************************************/
/*                                                                      */
/************************************************************************/
//here we create three types light --- direction,point and spot.
class DirectionLight : public Light
{
public:
    DirectionLight(DirectX::XMFLOAT4 Strength, DirectX::XMFLOAT3 Direction,const std::string& LightName = "DirectionalLight",
        int ShadowSize = 1024,ShadowTechnology Technology = ShadowTechnology::StandardShadowMap,const Camera* pMainCamera = nullptr);
    virtual ~DirectionLight();

    void SetSceneBoundingBox(const DirectX::BoundingBox& SceneAABB);

    virtual DirectX::XMMATRIX GetLightViewMatrix(int index = 0)const override;

    virtual DirectX::XMMATRIX GetLightProjMatrix()const override;

    virtual Camera* GetLightCamera(int index = 0)const override { return m_pLightCamera.get(); }

    virtual void SetStrength(DirectX::XMFLOAT4 Strength)override { m_Strength = Strength; };

    virtual void AdjustIntensity(float scaling)override;

    virtual void SetEnableState(bool state)override;

    virtual void SetSeletedState(bool seleted)override;

    virtual const LightConstants& GetLightConstant()override;

    virtual void SetShadowTechnology(ShadowTechnology Technology)override {};

    virtual void ResizeShadowMap(int newWidth, int newHeight)override {};

    virtual void SetShadowFormat(DXGI_FORMAT Format)override {};

    void SetDirection(DirectX::XMFLOAT3 Direction);
    //--------------------------------------------------------------------------
    const DirectX::XMFLOAT3 GetDirection()const { return m_Direction; }

    const DirectX::XMFLOAT4 GetStrength()const { return m_Strength; }

    float GetIntensity()const { return m_Intensity; } 

    const std::string& GetName()const { return m_lightname; }

    /**
     * Following functions are only for cascaded shadow map.
     */
    void SetCascadedLevel(CascadedShadow::CASCADED_LEVEL Level);

    void SetCascadedFitMethod(CascadedShadow::FIT_PROJECTION_TO_CASCADES Method);

    void SetCascadedNearFarFitMethod(CascadedShadow::FIT_TO_NEAR_FAR Method);

    //void SetMainCamera(const Camera* pCamera) { m_pMainCamera = pCamera; m_IsProjDirty = true; }
private:
    //A struct for triangle culling in aabb.
    struct Triangle
    {
        XMVECTOR pt[3];
        bool culled;
    };
    //Get eight points of this frustum in view space.
    //@param:EyeProjection:the projection matrix of this frustum
    //@param:the points view space positions
    void GetFrustumPointsViewSpace(DirectX::XMMATRIX EyeProjection, DirectX::XMVECTOR* FrustumPointsViewSpace)const;
    //Compute near and far place for a frustum with scene AABB
    //@see:https://learn.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps
    void ComputeNearAndFar(FLOAT& fNearPlane, FLOAT& fFarPlane, FXMVECTOR vLightCameraOrthographicMin,
        FXMVECTOR vLightCameraOrthographicMax, XMVECTOR* pvPointsInCameraView)const;
private:
    std::string m_lightname = "";

    DirectX::XMFLOAT4 m_Strength = { 1.0f,1.0f,1.0f,1.0f };
    DirectX::XMFLOAT3 m_Direction = { 1.0f,0.0f,0.0f };

    float m_Intensity = 1.0f; //Intensity scaling value between 0 and 1.
    BOOL m_Enable = TRUE;
    BOOL m_Selected = FALSE;

    LightConstants m_LightConstant;

    bool m_HasSetBoundingBox = false;
    DirectX::BoundingBox m_SceneAABB;
    //
    std::unique_ptr<Camera> m_pLightCamera;
    //the main camera to fit the light view frustum with camera view frustum.
    //In most cases,it will give more tighter light frustum.
    //@see:https://learn.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps
    //const Camera* m_pMainCamera;
    //bool m_IsFitViewFrustum;
    //
    mutable bool m_IsViewDirty;
    mutable bool m_IsProjDirty;
};
//--------------------------------------------------------------------------------
class SpotLight : public Light
{
public:
    SpotLight(DirectX::XMFLOAT4 Strength, DirectX::XMFLOAT3 Direction,DirectX::XMFLOAT4 Position ,float Range , float CosTheta ,const std::string& LightName = "SpotLight",int ShadowSize = 1024,ShadowTechnology Technology = StandardShadowMap);
    virtual ~SpotLight();

    virtual DirectX::XMMATRIX GetLightViewMatrix(int index = 0)const override;

    virtual DirectX::XMMATRIX GetLightProjMatrix()const override;

    virtual Camera* GetLightCamera(int index /* = 0 */)const override { return m_pLightCamera.get(); }

    virtual void SetStrength(DirectX::XMFLOAT4 Strength)override { m_Strength = Strength; };

    virtual void AdjustIntensity(float scaling)override;

    virtual void SetEnableState(bool state)override { m_Enable = state; };

    virtual void SetSeletedState(bool seleted)override { m_Selected = seleted; };

    void SetDirection(DirectX::XMFLOAT3 Direction);

    void SetPosition(DirectX::XMFLOAT4 Position) { m_Position = Position; m_IsViewDirty = true; };

    void SetRange(float Radius);

    void SetCosTheta(float CosTheta);

    virtual void SetShadowTechnology(ShadowTechnology Technology)override {};

    virtual void ResizeShadowMap(int newWidth, int newHeight)override {};

    virtual void SetShadowFormat(DXGI_FORMAT Format)override {};
//------------------------------------------------------------------------------
    const DirectX::XMFLOAT4& GetStrength()const { return m_Strength; }

    const DirectX::XMFLOAT3& GetDirection()const { return m_Direction; }

    const DirectX::XMFLOAT4& GetPosition()const { return m_Position; }

    float GetRange()const { return m_Range; }

    float GetCosTheta()const { return m_CosTheta; }

    const std::string& GetName()const { return m_lightname; }

    virtual const LightConstants& GetLightConstant()override;

private:
    std::string m_lightname = "";

    DirectX::XMFLOAT4 m_Strength = { 1.0f,1.0f,1.0f,1.0f };
    DirectX::XMFLOAT3 m_Direction = { 1.0f,0.0f,0.0f };
    DirectX::XMFLOAT4 m_Position = { 0.0f,0.0f,0.0f,1.0f };
    float m_Range = 5.0f;
    float m_CosTheta = 0.5f;

    float m_Intensity = 1.0f; //Intensity scaling value between 0 and 1.
    BOOL m_Enable = TRUE;
    BOOL m_Selected = FALSE;

    LightConstants m_LightConstant;

    std::unique_ptr<Camera> m_pLightCamera;
    //
    mutable bool m_IsViewDirty;
    mutable bool m_IsProjDirty;
};
//------------------------------------------------------------------
class PointLight : public Light
{
public:
    PointLight(DirectX::XMFLOAT4 Strength, DirectX::XMFLOAT4 Position, float Radius, const std::string& LightName = "PointLight", int ShadowSize = 1024, ShadowTechnology Technology = StandardShadowMap);
    virtual ~PointLight();
    //Get point light view matrix in six directions.
    //+X,-X,+Y,-Y,+Z,-Z
    virtual DirectX::XMMATRIX GetLightViewMatrix(int index /* = 0 */)const override;

    virtual DirectX::XMMATRIX GetLightProjMatrix()const override;

    virtual Camera* GetLightCamera(int index /* = 0 */)const override { return m_pLightCameras[index].get(); }

    virtual void SetStrength(DirectX::XMFLOAT4 Strength)override { m_Strength = Strength; };

    virtual void AdjustIntensity(float scaling)override;

    virtual void SetEnableState(bool state)override { m_Enable = state; };

    virtual void SetSeletedState(bool seleted)override { m_Selected = seleted; };

    void SetPosition(DirectX::XMFLOAT4 Position) { m_Position = Position; m_IsViewDirty = true; };

    void SetRange(float Radius);

    virtual void SetShadowTechnology(ShadowTechnology Technology)override {};

    virtual void ResizeShadowMap(int newWidth, int newHeight)override {};

    virtual void SetShadowFormat(DXGI_FORMAT Format)override {};

    //According to direction index to get specific light constant.
    //+X:0 -X:1 +Y:2 -Y:3 +Z:4 -Z:5
    virtual const LightConstants& GetLightConstant()override;
    //------------------------------------------------------------------------------
    const DirectX::XMFLOAT4& GetStrength()const { return m_Strength; }

    const DirectX::XMFLOAT4& GetPosition()const { return m_Position; }

    float GetRange()const { return m_Range; }

    const std::string& GetName()const { return m_lightname; }
;
private:
    std::string m_lightname = "";

    DirectX::XMFLOAT4 m_Strength = { 1.0f,1.0f,1.0f,1.0f };
    DirectX::XMFLOAT4 m_Position = { 0.0f,0.0f,0.0f,1.0f };
    float m_Range = 5.0f;

    LightConstants m_LightConstant;

    float m_Intensity = 1.0f; //Intensity scaling value between 0 and 1.
    BOOL m_Enable = TRUE;
    BOOL m_Selected = FALSE;

    //+X,-X,+Y,-Y,+Z,-Z 
    std::array<std::unique_ptr<Camera>, 6> m_pLightCameras;
    //
    mutable bool m_IsViewDirty;
    mutable bool m_IsProjDirty;
};