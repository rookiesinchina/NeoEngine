#include "Scenes.h"
#include <DirectXColors.h>
#include <NeoEngine/inc/Camera.h>
#include <NeoEngine/inc/Scene.h>
#include <NeoEngine/inc/Application.h>
#include <NeoEngine/inc/CommandQueue.h>
#include <NeoEngine/inc/RenderTarget.h>
#include <NeoEngine/inc/ResourceStateTracker.h>
#include <NeoEngine/inc/Texture.h>
#include <NeoEngine/inc/ModelLoader.h>
#include <NeoEngine/inc/CommandList.h>
#include <NeoEngine/inc/Window.h>
#include <NeoEngine/inc/imgui.h>
#include <NeoEngine/inc/Filter.h>


static void OnGUI();
static void HelpMarker(const char* desc);

static std::vector<std::string> gs_modelname;

static float gs_CameraSpeedDefault = 1.0f;
static float gs_CameraSpeed = gs_CameraSpeedDefault;

Scenes::Scenes(const std::wstring& GameName, int Width, int Height, bool vsync)
    :Game(GameName, Width, Height, vsync)
{
    m_pCamera = std::make_unique<Camera>();
    m_pCamera2 = std::make_unique<Camera>();
    m_AspectRadio = static_cast<float>(Width) / static_cast<float>(Height);

    m_pRenderTarget = std::make_shared<RenderTarget>();

    m_pEnvironmentPassConstant = std::make_unique<EnvironmentPassConstant>();
}

Scenes::~Scenes()
{}

bool Scenes::LoadContent()
{
    auto device = Application::GetApp()->GetDevice();
    auto commandQueue = Application::GetApp()->GetCommandQueue();
    auto commandList = commandQueue->GetCommandList();
    //
    m_pCamera->SetPosition(0.0f, 0.0f, -5.0f);
    m_pCamera->SetMainCamera(true);
    m_pCamera->SetLens(DirectX::XM_PIDIV2, m_AspectRadio, 0.1f, 20000.0f);

    m_pCamera2->SetPosition(0.0f, 0.0f, -5.0f);
    //m_pCamera2->SetMainCamera(true);
    m_pCamera2->SetLens(DirectX::XM_PIDIV4, m_AspectRadio, 0.1f, 200.0f);
    //
    Scene::Create();
    //
    //auto sponzaName = Scene::LoadModelFromFilePath("..\\Models\\CornellBox\\CornellBox-Original.obj", commandList);
    Scene::LoadModelFromFilePath("..\\Models\\newsponza\\sponza\\sponza.obj", commandList);
    //Scene::LoadModelFromFilePath("..\\Models\\home\\home.obj", commandList);
    Scene::GetScene()->SetWorldMatrix(DirectX::XMMatrixScaling(0.01f,0.01f,0.01f));
    //auto sceneName = Scene::LoadModelFromFilePath("..\\Models\\castle\\scene.obj", commandList, true);
    //Scene::GetScene()->SetWorldMatrix(DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f) * DirectX::XMMatrixRotationX(-DirectX::XM_PIDIV4) * DirectX::XMMatrixTranslation(-500.0f, 200.0f, 0.0f));
    //
    //add lights
    plight = Scene::AddDirectionalLight(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(1.0f, -1.0f, 1.0f), 
        "DirectionalLight0", 1024, ShadowTechnology::StandardShadowMap);
    //plight = Scene::AddPointLight();
    //plight = Scene::AddSpotLight(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(0.0f, 5.0f, 0.0f,1.0f),200.0f,20.0f);
    //Scene::AddSpotLight(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(-1.0, -1.0f, -1.0f), DirectX::XMFLOAT4(5.0f, 5.0f, 5.0f, 1.0f), 100.0f,0.5f,"SpotLight0",1024,ShadowTechnology::VarianceShadowMap);
    //auto light = Scene::AddPointLight("PointLight", DirectX::XMFLOAT4{ 1.0f,1.0f,1.0f,1.0f }, DirectX::XMFLOAT4{ -5.0f,0.0f,-8.0f,1.0f }, 5.0f,1024,ShadowTechnology::SATVarianceShadowMapINT);
    //add environment map
    Environment::Create(commandList);
    //Create a render target
    DXGI_FORMAT rendertargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    CD3DX12_CLEAR_VALUE ColorClear = { rendertargetFormat,DirectX::Colors::BlueViolet };

    DXGI_SAMPLE_DESC sampleDesc = Application::GetApp()->CheckMultipleSampleQulityLevels(rendertargetFormat, Application::GetApp()->m_MultiSampleCount);

    D3D12_RESOURCE_DESC rendertargetDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(rendertargetFormat, m_Width, m_Height, 1, 1,
            sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    Texture RenderTarget0 = Texture(&rendertargetDesc, &ColorClear, TextureUsage::RenderTargetTexture, L"RenderTarget0");
    //Then create a depth stencil texture
    DXGI_FORMAT depthstencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    CD3DX12_CLEAR_VALUE DepthClear = { depthstencilFormat,1.0f,0 };
    
    D3D12_RESOURCE_DESC depthstencilDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(depthstencilFormat, m_Width, m_Height, 1, 1,
            sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    Texture DepthStencil = Texture(&depthstencilDesc, &DepthClear, TextureUsage::Depth, L"DepthStencil");
    //
    m_pRenderTarget->AttachTexture(AttachmentPoint::Color0, RenderTarget0);
    m_pRenderTarget->AttachTexture(AttachmentPoint::DepthStencil, DepthStencil);

    m_pForwardRendering = std::make_unique<ForwardRendering>(ForwardPassType::OpaquePass, m_pRenderTarget,m_pCamera.get());
    m_pForwardRendering->SetPassInput(Scene::GetScene()->GetTypedModels());

    auto fence = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fence);

    return true;
}

bool Scenes::UnloadContent()
{
    Scene::Destroy();
    Environment::Destroy();
    return true;
}

void Scenes::Update(const UpdateEventArgs& UpdateArgs)
{
    Game::Update(UpdateArgs);

    m_pForwardRendering->UpdatePass(UpdateArgs);

    m_pCamera->Walk(UpdateArgs.ElapsedTime * 50.0f * m_CameraWalk * gs_CameraSpeed);
    m_pCamera->Strafe(UpdateArgs.ElapsedTime * 50.0f * m_CameraStrife * gs_CameraSpeed);
    //
    DirectX::XMStoreFloat4x4(&m_pEnvironmentPassConstant->ViewProj, DirectX::XMMatrixTranspose(m_pCamera->GetViewProj()));
    m_pEnvironmentPassConstant->EyePos = m_pCamera->GetPosition3f();
    Environment::GetEnvironment()->SetEnvironmentConstantBuffer(m_pEnvironmentPassConstant.get());
}

void Scenes::Render(const RenderEventArgs& RenderArgs)
{
    Game::Render(RenderArgs);

    auto commandQueue = Application::GetApp()->GetCommandQueue();
    auto commandList = commandQueue->GetCommandList();

    m_pForwardRendering->ExecutePass(commandList);
    //Render Environment
    //Environment::GetEnvironment()->RenderEnvironment(commandList);
    //m_pCamera2->RenderCameraFrustum(commandList, m_pCamera.get());
    plight->RenderLightFrustum(commandList, m_pCamera.get());

    commandQueue->ExecuteCommandList(commandList);
    //RenderGui
    OnGUI();

    m_pWindow->Present(&m_pRenderTarget->GetTexture(AttachmentPoint::Color0));
}

void Scenes::OnResize(const ResizeEventArgs& ResizeArgs)
{
    Game::OnResize(ResizeArgs);
    
    //Resize our off-screen texture and depth stencil buffer
    if (m_Width != ResizeArgs.Width || m_Height != ResizeArgs.Height)
    {
        m_pRenderTarget->Resize(ResizeArgs.Width, ResizeArgs.Height);

        m_AspectRadio = static_cast<float>(m_Width) / static_cast<float>(m_Height);

        m_pCamera->SetLens(DirectX::XM_PIDIV2, m_AspectRadio, 0.1f, 5000.0f);
    }
}

void Scenes::KeyPressed(const KeyEventArgs& KeyArgs)
{
Game::KeyPressed(KeyArgs);

switch (KeyArgs.Key)
{
case KeyCode::W:
    m_CameraWalk = 1.0f;
    break;
case KeyCode::S:
    m_CameraWalk = -1.0f;
    break;
case KeyCode::A:
    m_CameraStrife = -1.0f;
    break;
case KeyCode::D:
    m_CameraStrife = 1.0f;
    break;
case KeyCode::F:
    m_pWindow->ToggleFullScreenState();
    break;
case KeyCode::V:
    m_pWindow->ToggleVsyncState();
    break;
default:
    break;
}
}

void Scenes::KeyReleased(const KeyEventArgs& KeyArgs)
{
    Game::KeyReleased(KeyArgs);

    switch (KeyArgs.Key)
    {
    case KeyCode::W:
        m_CameraWalk = 0.0f;
        break;
    case KeyCode::S:
        m_CameraWalk = 0.0f;
        break;
    case KeyCode::A:
        m_CameraStrife = 0.0f;
        break;
    case KeyCode::D:
        m_CameraStrife = 0.0f;
        break;
    default:
        break;
    }
}

void Scenes::MouseButtonPressed(const MouseButtonEventArgs& MouseButtonArgs)
{
    Game::MouseButtonPressed(MouseButtonArgs);
}

void Scenes::MouseButtonReleased(const MouseButtonEventArgs& MouseButtonArgs)
{
    Game::MouseButtonReleased(MouseButtonArgs);

    m_CameraPitch = 0.0f;
    m_CameraHead = 0.0f;
}

void Scenes::MouseMove(const MouseMotionEventArgs& MouseMoveArgs)
{
    Game::MouseMove(MouseMoveArgs);

    if (MouseMoveArgs.RightButton)
    {
        m_pCamera->Head((MouseMoveArgs.RelX) * 0.003f);
        m_pCamera->Pitch((MouseMoveArgs.RelY) * 0.003f);
    }
}

static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void OnGUI()
{
    static bool showDemoWindow = false;

    static bool showModel = false;
    static bool showCamera = false;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::BeginMenu("Model"))
            {
                if (ImGui::BeginMenu("Other Models"))
                {
                    if (ImGui::MenuItem("Sponza"))
                    {
                        //Scene::GetScene()->SetRenderState(true, gs_modelname[0]);
                        //Scene::GetScene()->SetRenderState(false, gs_modelname[1]);
                    }
                    if (ImGui::MenuItem("Castle"))
                    {
                        //Scene::GetScene()->SetRenderState(false, gs_modelname[0]);
                        //Scene::GetScene()->SetRenderState(true, gs_modelname[1]);
                    }

                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

      
            ImGui::MenuItem("Camera", nullptr, &showCamera);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("GUI", true))
        {
            ImGui::MenuItem("About", nullptr, &showDemoWindow);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }


    if (showDemoWindow)
    {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }

    if (showModel)
    {

    }

    if (showCamera)
    {
        ImGui::Begin("Camera Information",&showCamera);

        ImGui::SliderFloat("Camera Motivation Speed", &gs_CameraSpeed, 0.0f, 20.0f);

        ImGui::End();
    }
}