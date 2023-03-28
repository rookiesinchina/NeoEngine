#include "GUI.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "Window.h"
#include "Application.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

#include <DirectXTex.h>

void GetSurfaceInfo(
    _In_ size_t width,
    _In_ size_t height,
    _In_ DXGI_FORMAT fmt,
    size_t* outNumBytes,
    _Out_opt_ size_t* outRowBytes,
    _Out_opt_ size_t* outNumRows);


GUI::GUI()
    : m_pFontTexture(nullptr)
    , m_pGUIRootSignature(nullptr)
    , m_GUIVS(nullptr)
    , m_GUIPS(nullptr)
    , m_d3d12GUIPipelineState(nullptr)
{};

GUI::~GUI()
{
    Destroy();
};

bool GUI::Initialize(const Window* pWindow)
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(pWindow->GetWindowHWND()))
    {
        MessageBox(NULL, L"ImGUI inilization failure", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    //load font texture
    int width, height;
    unsigned char* pixels = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    auto device = Application::GetApp()->GetDevice();
    auto commandQueue = Application::GetApp()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto commandList = commandQueue->GetCommandList();

    D3D12_RESOURCE_DESC fontResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);

    m_pFontTexture = std::make_unique<Texture>(&fontResourceDesc, nullptr, TextureUsage::Diffuse, L"GUI Font Texture");

    size_t row_pitch, slice_pitch;
    GetSurfaceInfo(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, &slice_pitch, &row_pitch, nullptr);

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = pixels;
    subResourceData.RowPitch = row_pitch;
    subResourceData.SlicePitch = slice_pitch;

    commandList->CopyTextureSubResource(m_pFontTexture.get(), 0, 1, &subResourceData);
    commandList->GenerateMipMaps(m_pFontTexture.get());

    commandQueue->ExecuteCommandList(commandList);
    //Create root signature
    D3D12_FEATURE_DATA_ROOT_SIGNATURE rootVersion = {};
    rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootVersion, sizeof(rootVersion))))
    {
        rootVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    CD3DX12_DESCRIPTOR_RANGE1 descriptorRange = {};
    descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[GUIRootParameter::GuiNumRootParameter];
    rootParameters[GUIRootParameter::GuiConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[GUIRootParameter::GuiFontTexture].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC fontLinearSampler = {};
    fontLinearSampler.Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    fontLinearSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_FLAGS rootSigFlag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                            D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
                                            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                                            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                            D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSigDesc = {};
    versionRootSigDesc.Init_1_1(GUIRootParameter::GuiNumRootParameter, rootParameters, 1, &fontLinearSampler, rootSigFlag);

    m_pGUIRootSignature = std::make_unique<RootSignature>(versionRootSigDesc.Desc_1_1, rootVersion.HighestVersion);
    //Create Pipeline State.
    const D3D12_INPUT_ELEMENT_DESC InputElement[] =
    {
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,  0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,  0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",   0,DXGI_FORMAT_R8G8B8A8_UNORM,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };

    m_GUIVS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\GUI.hlsl", nullptr, "VS", "vs_5_1");
    m_GUIPS = d3dUtil::CompileShader(L"..\\NeoEngine\\Shaders\\GUI.hlsl", nullptr, "PS", "ps_5_1");

    CD3DX12_BLEND_DESC BlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    BlendDesc.AlphaToCoverageEnable = FALSE;
    BlendDesc.IndependentBlendEnable = FALSE;
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;

    CD3DX12_DEPTH_STENCIL_DESC DepthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    DepthStencilDesc.DepthEnable = FALSE;
    DepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    CD3DX12_RASTERIZER_DESC RasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    RasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

    D3D12_RT_FORMAT_ARRAY RtArray = {};
    RtArray.NumRenderTargets = 1;
    RtArray.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    struct GUIPipelineState
    {
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendDesc;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencilDesc;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerDesc;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveType;
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargets;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    }GuiPipelineState;

    GuiPipelineState.BlendDesc = BlendDesc;
    GuiPipelineState.DepthStencilDesc = DepthStencilDesc;
    GuiPipelineState.InputLayout = { InputElement,_countof(InputElement) };
    GuiPipelineState.PrimitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    GuiPipelineState.pRootSignature = m_pGUIRootSignature->GetRootSignature().Get();
    GuiPipelineState.RenderTargets = RtArray;
    GuiPipelineState.VS = CD3DX12_SHADER_BYTECODE(m_GUIVS.Get());
    GuiPipelineState.PS = CD3DX12_SHADER_BYTECODE(m_GUIPS.Get());
    GuiPipelineState.RasterizerDesc = RasterizerDesc;
    GuiPipelineState.SampleDesc = { 1,0 };

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStreamDesc = { sizeof(GuiPipelineState), &GuiPipelineState };
    ThrowIfFailed(device->CreatePipelineState(&pipelineStreamDesc, IID_PPV_ARGS(&m_d3d12GUIPipelineState)));

    return true;
}

void GUI::UpdateGUI()
{
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void GUI::RenderGUI(std::shared_ptr<CommandList> commandList,const RenderTarget& rendertarget)
{
    ImGui::Render();

    auto GuiData =  ImGui::GetDrawData();

    if (!GuiData || GuiData->CmdListsCount == 0)
        return;

    CD3DX12_VIEWPORT viewport(GuiData->DisplayPos.x, GuiData->DisplayPos.y, GuiData->DisplaySize.x, GuiData->DisplaySize.y);

    commandList->SetD3D12ViewPort(&viewport);

    commandList->SetGraphicsRootSignature(m_pGUIRootSignature.get());
    commandList->SetD3D12PipelineState(m_d3d12GUIPipelineState);
    commandList->SetRenderTargets(rendertarget);

    //set gui constant buffer
    ConstantBuffer constantbuffer;

    float R = GuiData->DisplayPos.x + GuiData->DisplaySize.x;
    float L = GuiData->DisplayPos.x;
    float T = GuiData->DisplayPos.y;
    float B = GuiData->DisplayPos.y + GuiData->DisplaySize.y;

    constantbuffer.Proj = {
        2.0f / (R - L),                  0.0f,          0.0f,          0.0f,
        0.0f,                  2.0f / (T - B),          0.0f,          0.0f,
        0.0f,                            0.0f,          0.5f,          0.0f,
        (L + R) / (L - R),  (B + T) / (B - T),          0.5f,          1.0f
    };

    DirectX::XMStoreFloat4x4(&constantbuffer.Proj, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&constantbuffer.Proj)));

    commandList->SetGraphicsDynamicConstantBuffer(GUIRootParameter::GuiConstantBuffer, constantbuffer);
    commandList->SetShaderResourceView(GUIRootParameter::GuiFontTexture, 0, m_pFontTexture.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const DXGI_FORMAT indexFormat = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

    for (int i = 0; i < GuiData->CmdListsCount; ++i)
    {
        const ImDrawList* drawList = GuiData->CmdLists[i];

        commandList->SetDynamicVertexBuffer(0, drawList->VtxBuffer.size(), sizeof(ImDrawVert), drawList->VtxBuffer.Data);
        commandList->SetDynamicIndexBuffer(drawList->IdxBuffer.size(), indexFormat, drawList->IdxBuffer.Data);


        int indexOffset = 0;
        for (int j = 0; j < drawList->CmdBuffer.size(); ++j)
        {
            const ImDrawCmd& drawCmd = drawList->CmdBuffer[j];
            if (drawCmd.UserCallback)
            {
                drawCmd.UserCallback(drawList, &drawCmd);
            }
            else
            {
                ImVec4 clipRect = drawCmd.ClipRect;
                D3D12_RECT scissorRect;
                scissorRect.left = static_cast<LONG>(clipRect.x - GuiData->DisplayPos.x);
                scissorRect.top = static_cast<LONG>(clipRect.y - GuiData->DisplayPos.y);
                scissorRect.right = static_cast<LONG>(clipRect.z - GuiData->DisplayPos.x);
                scissorRect.bottom = static_cast<LONG>(clipRect.w - GuiData->DisplayPos.y);

                if (scissorRect.right - scissorRect.left > 0.0f &&
                    scissorRect.bottom - scissorRect.top > 0.0)
                {
                    commandList->SetD3D12ScissorRect(&scissorRect);
                    commandList->DrawIndexed(drawCmd.ElemCount, 1, indexOffset, 0, 0);
                }
            }
            indexOffset += drawCmd.ElemCount;
        }
    }
}

void GUI::Destroy()
{
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

//--------------------------------------------------------------------------------------
// Get surface information for a particular format
//--------------------------------------------------------------------------------------
void GetSurfaceInfo(
    _In_ size_t width,
    _In_ size_t height,
    _In_ DXGI_FORMAT fmt,
    size_t* outNumBytes,
    _Out_opt_ size_t* outRowBytes,
    _Out_opt_ size_t* outNumRows)
{
    size_t numBytes = 0;
    size_t rowBytes = 0;
    size_t numRows = 0;

    bool bc = false;
    bool packed = false;
    bool planar = false;
    size_t bpe = 0;
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        bc = true;
        bpe = 8;
        break;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        bc = true;
        bpe = 16;
        break;

    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_YUY2:
        packed = true;
        bpe = 4;
        break;

    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        packed = true;
        bpe = 8;
        break;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
        planar = true;
        bpe = 2;
        break;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        planar = true;
        bpe = 4;
        break;
    }

    if (bc)
    {
        size_t numBlocksWide = 0;
        if (width > 0)
        {
            numBlocksWide = std::max<size_t>(1, (width + 3) / 4);
        }
        size_t numBlocksHigh = 0;
        if (height > 0)
        {
            numBlocksHigh = std::max<size_t>(1, (height + 3) / 4);
        }
        rowBytes = numBlocksWide * bpe;
        numRows = numBlocksHigh;
        numBytes = rowBytes * numBlocksHigh;
    }
    else if (packed)
    {
        rowBytes = ((width + 1) >> 1) * bpe;
        numRows = height;
        numBytes = rowBytes * height;
    }
    else if (fmt == DXGI_FORMAT_NV11)
    {
        rowBytes = ((width + 3) >> 2) * 4;
        numRows = height * 2; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
        numBytes = rowBytes * numRows;
    }
    else if (planar)
    {
        rowBytes = ((width + 1) >> 1) * bpe;
        numBytes = (rowBytes * height) + ((rowBytes * height + 1) >> 1);
        numRows = height + ((height + 1) >> 1);
    }
    else
    {
        size_t bpp = DirectX::BitsPerPixel(fmt);
        rowBytes = (width * bpp + 7) / 8; // round up to nearest byte
        numRows = height;
        numBytes = rowBytes * height;
    }

    if (outNumBytes)
    {
        *outNumBytes = numBytes;
    }
    if (outRowBytes)
    {
        *outRowBytes = rowBytes;
    }
    if (outNumRows)
    {
        *outNumRows = numRows;
    }
}