#pragma once
#include <DirectXCollision.h>

#include "ModelLoader.h"
#include "Camera.h"

//@brief: a class for frustum culling.This class can help us to cull unnecessary mesh before IA(input and assemble) so that 
//it will improve performance of rendering.It is very useful when rendering and shadow.

class Model;

class FrustumCullinger
{
public:
    FrustumCullinger();
    FrustumCullinger(Camera* camera);

    ~FrustumCullinger();
    //Bind a camera to get frustum aabb
    void BindFrustumCamera(const Camera* camera);
    //Bind a model and use meshes in this model to test if this mesh will be culled.
    void BindModelCulled(const Model* pModel);
    //Get a mesh cull state.
    //NOTE:you MUST make sure that this mesh is in binded model!
    bool IsCull(const ModelSpace::Mesh* pMesh);
    //Open or Close culling.
    void SetCullingState(bool IsOpenCulling) { m_IsOpenCulling = IsOpenCulling; };
private:
    const Camera* m_FrustumCamera;
    const Model* m_pModel;
    bool m_IsOpenCulling;
    //
    bool m_IsBindCamera;
};
