#pragma once

#include <wrl.h>
#include <memory>

#include "d3dx12.h"


//@brief:a class for generating SAT by using compute shader
class RootSignature;
class Texture;
class CommandList;

class GenerateSAT
{
public:
    GenerateSAT(const Texture* pInputTexture);
    ~GenerateSAT() {};

    void GenerateSATs(std::shared_ptr<CommandList> commandList);

    const Texture* GetSATs()const { return m_pPingPong1Texture.get(); }
private:
    std::unique_ptr<Texture> m_pPingPong0Texture;
    std::unique_ptr<Texture> m_pPingPong1Texture;
    const Texture* m_pTexture;

    std::unique_ptr<RootSignature> m_pRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12SumInGroupPipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12SATPipelineState;
};