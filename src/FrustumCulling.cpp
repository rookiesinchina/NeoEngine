#include "FrustumCulling.h"
#include "Model.h"

FrustumCullinger::FrustumCullinger()
{
    m_FrustumCamera = nullptr;
    m_IsOpenCulling = true;
    m_IsBindCamera = false;
}

FrustumCullinger::FrustumCullinger(Camera* camera)
    :m_FrustumCamera(camera)
    , m_IsOpenCulling(true)
    , m_IsBindCamera(true)
{};

FrustumCullinger::~FrustumCullinger() {};

void FrustumCullinger::BindFrustumCamera(const Camera* camera)
{
    m_FrustumCamera = camera;
    m_IsBindCamera = true;
}

void FrustumCullinger::BindModelCulled(const Model* pModel)
{
    if (m_IsOpenCulling && pModel)
    {
        m_pModel = pModel;
        //For perspective camera.
        if (m_FrustumCamera->GetCameraStyle() == CameraStyle::Perspective)
        {
            //we need to transform frustum aabb to model local space
            DirectX::BoundingFrustum frustum;
            m_FrustumCamera->GetCameraFrustum(frustum);

            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&m_pModel->GetWorldMatrix4x4f());

            DirectX::XMMATRIX InvViewWorld =
                m_FrustumCamera->GetInvView() * DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(world), world);
            //
            DirectX::BoundingFrustum localFrustum;
            frustum.Transform(localFrustum, InvViewWorld);

            for (auto& mesh : m_pModel->m_ModelLoader->Meshes())
            {
                if (localFrustum.Contains(mesh.mMeshAABB) != DirectX::DISJOINT)
                {
                    mesh.m_IsCulled = false;
                }
                else
                {
                    mesh.m_IsCulled = true;
                }
            }
        }
        //For orthographic camera.
        else
        {
            DirectX::BoundingBox frustum;
            m_FrustumCamera->GetCameraFrustum(frustum);
            //transform aabb from camera space to local space.
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&m_pModel->GetWorldMatrix4x4f());

            DirectX::XMMATRIX InvViewWorld =
                m_FrustumCamera->GetInvView() * DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(world), world);
            //
            DirectX::BoundingBox localFrustum;
            frustum.Transform(localFrustum, InvViewWorld);

            for (auto& mesh : m_pModel->m_ModelLoader->Meshes())
            {
                if (localFrustum.Contains(mesh.mMeshAABB) != DirectX::DISJOINT)
                {
                    mesh.m_IsCulled = false;
                }
                else
                {
                    mesh.m_IsCulled = true;
                }
            }
        }
    }
}

bool FrustumCullinger::IsCull(const ModelSpace::Mesh* pMesh)
{
    if (m_IsOpenCulling)
    {
        assert(m_pModel && m_IsBindCamera && "You need to bind model and camera firstly!");
        return pMesh->m_IsCulled;
    }
    return false;
}