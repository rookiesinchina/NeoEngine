#pragma once

#include <memory>
#include <wrl.h>
#include "d3dx12.h"
#include <DirectXMath.h>

class Window;
class RootSignature;
class Texture;
class CommandList;
class RenderTarget;

class GUI
{
public:
    enum GUIRootParameter
    {
        GuiConstantBuffer,
        GuiFontTexture,
        GuiNumRootParameter
    };

    struct ConstantBuffer
    {
        DirectX::XMFLOAT4X4 Proj; //an orthographic projection matrix for gui
    };


    GUI();
    ~GUI();

    //Initilize ImGUI context and create font texture,root signature and pipeline state.
    bool Initialize(const Window* pWindow);

    void UpdateGUI();

    void RenderGUI(std::shared_ptr<CommandList> commandList,const RenderTarget& rendertarget);

private:
    void Destroy();

    std::unique_ptr<RootSignature> m_pGUIRootSignature;
    //Font texture.For now,our GUI do not support multi-font.
    std::unique_ptr<Texture> m_pFontTexture;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12GUIPipelineState;

    Microsoft::WRL::ComPtr<ID3DBlob> m_GUIVS;
    Microsoft::WRL::ComPtr<ID3DBlob> m_GUIPS;
};