#pragma once
#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>
#include <string>

#include "DescriptorAllocation.h"
#include "CommandList.h"
#include "Mesh.h"

//here we create a skybox for scene

//@brief: a class is responsible for managing all environment maps.



struct EnvironmentPassConstant
{
    DirectX::XMFLOAT4X4 ViewProj;
    DirectX::XMFLOAT3   EyePos;
    float               Padding;
};

class RootSignature;
class Texture;
class Mesh;

enum EnvirRootParameters
{
    ConstantBuffer,
    ConstantPassBuffer,
    EnvironmentMaps,
    NumEnvirRootParameters
};

class Environment
{
public:
    const static UINT m_MaxEnvironmentNum = 10;

    Environment(std::shared_ptr<CommandList> commandList);
    ~Environment();

    static void Create(std::shared_ptr<CommandList> commandList);

    static std::wstring AddNewEnvironmentMap(const std::wstring& filepath, std::shared_ptr<CommandList> commandList);
    static void EraseEnvironmentMap(const std::wstring& mapName);

    static void Destroy();

    static Environment* GetEnvironment() { return ms_pEnvironmnet; }

    void RenderEnvironment(std::shared_ptr<CommandList> commandList, const std::wstring& mapName = L"");

    void SetEnvironmentConstantBuffer(EnvironmentPassConstant* EnvironmentPassCb);

private:
    struct EnvironmentConstant
    {
        DirectX::XMFLOAT4X4 World;
    };

    static Environment* ms_pEnvironmnet;

    std::unique_ptr<RootSignature> m_pRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12PipelineState;

    Microsoft::WRL::ComPtr<ID3DBlob> m_VS;
    Microsoft::WRL::ComPtr<ID3DBlob> m_PS;

    static std::unordered_map<std::wstring, std::shared_ptr<Texture>> m_EnvironmentMaps;
    static std::vector < std::shared_ptr<Texture>> m_EnvironmentArray;

    SIZE_T m_CurrentEnvironmentIndex;

    std::unique_ptr<Mesh> m_SkyMesh;

    std::unique_ptr<EnvironmentConstant> m_pEnvironmentConstant;
    EnvironmentPassConstant* m_pEnvironmentPassConstant;
};