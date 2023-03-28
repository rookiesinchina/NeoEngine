#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>
#include <DirectXCollision.h>

#include "d3dUtil.h"


namespace ModelSpace
{
    struct Vertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 Tangent;
        DirectX::XMFLOAT2 TexC;
    };

    const D3D12_INPUT_ELEMENT_DESC ModelInputElemets[] =
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",  0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TANGENT", 0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,   0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };

    struct MeshMaterial
    {
        DirectX::XMFLOAT4 DiffuseColor = { 0.0f,0.0f,0.0f,1.0f };
        DirectX::XMFLOAT4 SpecularColor = { 0.0f,0.0f,0.0f,1.0f };
        DirectX::XMFLOAT4 AmbientColor = { 0.0f,0.0f,0.0f,1.0f };
        DirectX::XMFLOAT4 EmissiveColor = { 0.0f,0.0f,0.0f,1.0f };
        float SpecularExponent = 1.0f;
        float TransparentFactor = 0.0f;   //0 means fully opaque and 1 means totally transparent.
        float Ior = 1.0f;                 //Index of refractor ---- it should be greater than 1.0f.
    };


    struct Mesh
    {
    public:
        std::string mMeshName;
        std::vector<Vertex> mVertices;
        std::vector<uint32_t> mIndices;
        //A map for all texture in this mesh
        //Key:TextureUsage--albedo/normal/specular....
        //Value:a vector for file load of all texture in this usage.
        using TextureUsagePath = std::unordered_map<TextureUsage, std::vector<std::string>>;

        TextureUsagePath mTextureUsagePath;
        MeshMaterial mMeshMaterial;
        bool mbHasMaterial;
        //mesh aabb for frustum culling and other algorithm.
        //Note:AABB is in local space of this mesh.
        DirectX::BoundingBox mMeshAABB;
        mutable bool m_IsCulled = false;

        UINT mVertexOffset = 0;
        UINT mIndexOffset = 0;

        Mesh(const std::vector<Vertex>& vertices,
            const std::vector<uint32_t>& indices,
            const TextureUsagePath& textureUsagePath,
            const MeshMaterial& meshMaterial,
            bool hasMaterial,
            DirectX::BoundingBox AABB,
            const std::string& name,
            UINT vertexOffset,
            UINT indexOffset)
        {
            mVertices = vertices;
            mIndices = indices;
            mTextureUsagePath = textureUsagePath;
            mMeshMaterial = meshMaterial;
            mbHasMaterial = hasMaterial;
            mMeshAABB = AABB;
            mMeshName = name;
            mVertexOffset = vertexOffset;
            mIndexOffset = indexOffset;
        }
        
        Mesh(const Mesh& copy);
        Mesh& operator=(const Mesh& assign);
        Mesh(Mesh&& move)noexcept;
        Mesh& operator=(Mesh&& move)noexcept;

        ~Mesh() {};

    protected:
        void free() {
            mMeshName.clear();
            mVertices.clear();
            mIndices.clear();
            mTextureUsagePath.clear();
            mVertexOffset = mIndexOffset = 0;
        };
    };

    class ModelLoader
    {

    private:
        std::vector<Mesh> mMeshes;
        std::string mModelName;

        std::string mDirectory;

        //One model maybe has many meshes.
        //Different Mesh has different textures.
        //We use map to represent all textures indice in TextureMapPath for a specific mesh.
        //Key:a specific mesh name in this model value:all textures index of a specific usage in a specific mesh
        std::unordered_map<std::string, UINT> mTexturesMapIndex[TextureUsage::NumTextureUsage];
        //texture path (not repeated)
        //a vetor array for all textures in this model
        //Different usage textures are put into different vector.
        //We want all different usage textures are put one containers in this model.
        std::vector<std::string> mTexturesMapPath[TextureUsage::NumTextureUsage];

        //A offset value for vertex and index start position of next mesh.
        UINT mCurrVertexOffsetStart = 0;
        UINT mCurrIndexOffsetStart = 0;

        void LoadModel(std::string path);
        void ProcessNode(aiNode* node, const aiScene* scene);
        Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
        std::vector<std::string> LoadMaterialTextures(aiMaterial* mat, aiTextureType type);
        /**
         * Create indice for all meshes and put all same usage textures in one heap.
         */
        void CreateTexturesIndex();

    public:
        ModelLoader(const std::string& path)
        {
            LoadModel(path);
            CreateTexturesIndex();
        }
        ModelLoader(const ModelLoader&) = delete;
        ModelLoader& operator=(const ModelLoader&) = delete;
        ~ModelLoader() {};

        const std::vector<Mesh>& Meshes()const
        {
            return mMeshes;
        }

        const std::string& ModelName()const
        {
            return mModelName;
        }

        const std::string& Directory()const
        {
            return mDirectory;
        }
        //Get a specific usage textures index in heap of all mesh in this model
        //@param: Texture Usage.
        //@return: a map which represents all indice of all mesh under a specific usage.
        std::unordered_map<std::string, UINT> GetTextureMapIndex(const TextureUsage& Usage)const
        {
            return mTexturesMapIndex[Usage];
        }
        /**
         * Get a vector which represents all texture paths of specific usage in this model.
         * @param: Texture Usage.
         * @return: all texture paths for a specific usage.
         */
        const std::vector<std::string>& GetTextureMapPath(const TextureUsage& Usage)const
        {
            return mTexturesMapPath[Usage];
        }
    };
};