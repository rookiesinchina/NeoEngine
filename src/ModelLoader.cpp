#include "ModelLoader.h"

ModelSpace::Mesh::Mesh(const ModelSpace::Mesh& copy)
    :mMeshName(copy.mMeshName)
    , mVertices(copy.mVertices)
    , mIndices(copy.mIndices)
    , mTextureUsagePath(copy.mTextureUsagePath)
    , mbHasMaterial(copy.mbHasMaterial)
    , mMeshAABB(copy.mMeshAABB)
    , mMeshMaterial(copy.mMeshMaterial)
    , mVertexOffset(copy.mVertexOffset)
    , mIndexOffset(copy.mIndexOffset)
    , m_IsCulled(copy.m_IsCulled)
{};

ModelSpace::Mesh& ModelSpace::Mesh::operator=(const ModelSpace::Mesh& assign)
{
    if (this != &assign)
    {
        free();

        mMeshName = assign.mMeshName;
        mVertices = assign.mVertices;
        mIndices = assign.mIndices;
        mTextureUsagePath = assign.mTextureUsagePath;
        mMeshMaterial = assign.mMeshMaterial;
        mbHasMaterial = assign.mbHasMaterial;
        mMeshAABB = assign.mMeshAABB;
        mVertexOffset = assign.mVertexOffset;
        mIndexOffset = assign.mIndexOffset;
        m_IsCulled = assign.m_IsCulled;
    }
    return *this;
}

ModelSpace::Mesh::Mesh(ModelSpace::Mesh&& move)noexcept
    :mMeshName(std::move(move.mMeshName))
    , mVertices(std::move(move.mVertices))
    , mIndices(std::move(move.mIndices))
    , mTextureUsagePath(std::move(move.mTextureUsagePath))
    , mVertexOffset(std::move(move.mVertexOffset))
    , mIndexOffset(std::move(move.mIndexOffset))
{
    mMeshMaterial = move.mMeshMaterial;
    mbHasMaterial = move.mbHasMaterial;
    mMeshAABB = move.mMeshAABB;
    m_IsCulled = move.m_IsCulled;
    move.free();
};

ModelSpace::Mesh& ModelSpace::Mesh::operator=(ModelSpace::Mesh&& move)noexcept
{
    if (this != &move)
    {
        free();

        mMeshName = std::move(move.mMeshName);
        mVertices = std::move(move.mVertices);
        mIndices = std::move(move.mIndices);
        mTextureUsagePath = std::move(move.mTextureUsagePath);
        mMeshMaterial = move.mMeshMaterial;
        mbHasMaterial = move.mbHasMaterial;
        mMeshAABB = move.mMeshAABB;
        mVertexOffset = std::move(move.mVertexOffset);
        mIndexOffset = std::move(move.mIndexOffset);
        m_IsCulled = move.m_IsCulled;

        move.free();
    }
    return *this;
}

void ModelSpace::ModelLoader::LoadModel(std::string path)
{
    Assimp::Importer import;
    //Here,for Direct3D,we need to configure LeftHand and generate normal & tangent for bump map
     const aiScene* scene = import.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenNormals |
        aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::string error = "ERROR:ASSIMP: ";
        error += import.GetErrorString();
        OutputDebugStringA(error.c_str());
        return;
    }
    mModelName = path.substr(path.find_last_of('\\'));
    mDirectory = path.substr(0, path.find_last_of('\\')) + "\\";


    ProcessNode(scene->mRootNode, scene);
}

void ModelSpace::ModelLoader::ProcessNode(aiNode* node, const aiScene* scene)
{
    //for each node,we search all meshes in this node,then add mesh to mMeshes
    for (UINT i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        //we need to reinterpret mesh info to our self-defined data type
        mMeshes.push_back(ProcessMesh(mesh, scene));
    }
    //repeat process for all node
    for (UINT i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNode(node->mChildren[i], scene);
    }
}

ModelSpace::Mesh ModelSpace::ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    ModelSpace::Mesh::TextureUsagePath textureUsagePath;
    ModelSpace::MeshMaterial meshmaterial;
    bool hasMaterial = false;
    DirectX::BoundingBox aabb;

    std::string meshname = mesh->mName.C_Str();
    UINT curVertexOffset = mCurrVertexOffsetStart;
    UINT curIndexOffset = mCurrIndexOffsetStart;

    //save vertex info
    for (UINT i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex vertex;
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT3 tangent;
        DirectX::XMFLOAT2 tex;
        pos.x = mesh->mVertices[i].x;
        pos.y = mesh->mVertices[i].y;
        pos.z = mesh->mVertices[i].z;

        if (mesh->HasNormals())
        {
            normal.x = mesh->mNormals[i].x;
            normal.y = mesh->mNormals[i].y;
            normal.z = mesh->mNormals[i].z;
        }
        if (mesh->mTextureCoords[0])
        {
            tex.x = mesh->mTextureCoords[0][i].x;
            tex.y = mesh->mTextureCoords[0][i].y;

            tangent.x = mesh->mTangents[i].x;
            tangent.y = mesh->mTangents[i].y;
            tangent.z = mesh->mTangents[i].z;
        }
        else
        {
            tex = { 0.0f,0.0f };
            tangent = { 0.0f,0.0f,0.0f };
        }
        vertex.Position = pos;
        vertex.Normal = normal;
        vertex.Tangent = tangent;
        vertex.TexC = tex;

        vertices.push_back(vertex);
    }
    //After all vertice finished,we create aabb for this mesh
    DirectX::BoundingBox::CreateFromPoints(aabb, vertices.size(), (DirectX::XMFLOAT3*)vertices.data(), sizeof(Vertex));

    mCurrVertexOffsetStart += mesh->mNumVertices;
    //save index info
    for (UINT i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];
        for (UINT j = 0; j < face.mNumIndices; ++j)
        {
            indices.push_back(face.mIndices[j]);
        }
        mCurrIndexOffsetStart += face.mNumIndices;
    }
    //save materal info
    if (mesh->mMaterialIndex >= 0)
    {
        //according to this mesh's matIndex to get materal from the whole matrialarray in scene
        //For now,we temporarily support three types texture input,but we can easily add more texture types.
        aiColor3D diffusecolor(0.0f, 0.0f, 0.0f);
        aiColor3D ambientColor(0.0f, 0.0f, 0.0f);
        aiColor3D specularColor(0.0f, 0.0f, 0.0f);
        aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
        hasMaterial = true;

        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        //Diffuse Color----Kd
        material->Get(AI_MATKEY_COLOR_DIFFUSE, diffusecolor);
        meshmaterial.DiffuseColor = { diffusecolor.r,diffusecolor.g,diffusecolor.b,1.0f };
        //specular color---Ks
        material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor);
        meshmaterial.SpecularColor = { specularColor.r,specularColor.g,specularColor.b,1.0f };
        //ambient color----Ka
        material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor);
        meshmaterial.AmbientColor = { ambientColor.r,ambientColor.g,ambientColor.b,1.0f };
        //emissive color---ke
        material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
        meshmaterial.EmissiveColor = { emissiveColor.r,emissiveColor.g,emissiveColor.b,1.0f };
        //specular exponent---Ns
        material->Get(AI_MATKEY_SHININESS, meshmaterial.SpecularExponent);
        //transparent----Tr
        material->Get(AI_MATKEY_OPACITY, meshmaterial.TransparentFactor);
        //index of refractor/ior ---Ni
        material->Get(AI_MATKEY_REFRACTI, meshmaterial.Ior);

        textureUsagePath[TextureUsage::Diffuse] = LoadMaterialTextures(material, aiTextureType_DIFFUSE);

        textureUsagePath[TextureUsage::Specular] = LoadMaterialTextures(material, aiTextureType_SPECULAR);
        //For MTL file,it expects height map nor normal map.
        textureUsagePath[TextureUsage::HeightMap] = LoadMaterialTextures(material, aiTextureType_HEIGHT);//I do not know why the flag of normal map is Height...It doesn't make sense...

        textureUsagePath[TextureUsage::NormalMap] = LoadMaterialTextures(material, aiTextureType_NORMALS);

        textureUsagePath[TextureUsage::Ambient] = LoadMaterialTextures(material, aiTextureType_AMBIENT);

        textureUsagePath[TextureUsage::Opacity] = LoadMaterialTextures(material, aiTextureType_OPACITY);

        textureUsagePath[TextureUsage::Emissive] = LoadMaterialTextures(material, aiTextureType_EMISSIVE);
    }

    return Mesh(vertices, indices, textureUsagePath, meshmaterial, hasMaterial, aabb, meshname, curVertexOffset, curIndexOffset);
}

std::vector<std::string> ModelSpace::ModelLoader::LoadMaterialTextures(aiMaterial* mat,
    aiTextureType type)
{
    std::vector<std::string> texturespath;
    for (UINT i = 0; i < mat->GetTextureCount(type); ++i)
    {
        aiString str;
        mat->GetTexture(type, i, &str);

        texturespath.push_back(str.C_Str());
    }
    return texturespath;
}

void ModelSpace::ModelLoader::CreateTexturesIndex()
{
    UINT textureMapIndexOffset[TextureUsage::NumTextureUsage] = { 0 };
    //texture_path,index
    //Here we need this variable to help us to record texture path and its index in specific usage container.
    //We can use it to get index for texture which is used many times.
    std::unordered_map<std::string, UINT> texture_loaded[TextureUsage::NumTextureUsage];

    //for each mesh in this model
    for (const auto& mesh : mMeshes)
    {
        //Each mesh has different texture usage and texture path
        //Note:One texture path may be used in diffrent meshes.
        for (const auto& usagePath : mesh.mTextureUsagePath)
        {
            //For each texture usage
            for (int usage = 0; usage < TextureUsage::NumTextureUsage; ++usage)
            {
                //If this mesh has this usage texture,we need to put them into one container.
                if (usagePath.first == usage && usagePath.second.size() != 0)
                {
                    //We check each texture by texture path.
                    for (const auto& path : usagePath.second)
                    {
                        //check if texture has been loaded
                        auto iterPos = texture_loaded[usage].find(path);
                        if (iterPos != texture_loaded[usage].end())
                        {
                            //If this texture has been loaded which indicates that this texture also be uesd by other mesh.
                            //So we can just get this texture index for this mesh.
                            mTexturesMapIndex[usage].insert({ mesh.mMeshName,iterPos->second });
                        }
                        else//If this texture is a new one.we push this to container.
                        {
                            mTexturesMapIndex[usage].insert({ mesh.mMeshName, textureMapIndexOffset[usage] });
                            mTexturesMapPath[usage].push_back(path);
                            //Fresh loaded map for next loading.
                            texture_loaded[usage].insert({ path, textureMapIndexOffset[usage] });
                            textureMapIndexOffset[usage]++;
                        }
                    }
                }
            }
        }
    }
}

