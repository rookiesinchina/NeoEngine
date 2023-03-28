#include "Window.h"
#include "Application.h"
#include "Game.h"
#include "CommandQueue.h"
#include "GameTimer.h"
#include "d3dUtil.h"
#include "ResourceStateTracker.h"
#include "CommandList.h"
#include "GUI.h"



Window::Window(HWND hwnd,const std::wstring& windowName,int width, int height, bool vsync)
{
    m_Hwnd = hwnd;
    m_WindowName = windowName;
    m_Width = width;
    m_Height = height;
    m_Vsync = vsync;
    m_FullScreenState = false;
    m_PreviousMouseX = 0.0f;
    m_PreviousMouseY = 0.0f;

    m_dxgiSwapChain = CreateSwapChain();

    for (int i = 0; i < m_BackBufferCount; ++i)
    {
        m_BackBufferTextures[i] = std::make_unique<Texture>(TextureUsage::RenderTargetTexture,L"BackBuffer[" + std::to_wstring(i) + L"]");
        m_FenceValue[i] = 0;
        m_FrameCount[i] = 0;
    }

    auto App = Application::GetApp();
    // Create render target descriptor heap
    m_SupportTearing = App->GetTearingState();
    // Create resource and render target view
    UpdateRenderTargetView();
    //Initialize GUI
    m_pGUI = std::make_unique<GUI>();
    m_pGUI->Initialize(this);
}

void Window::Show()
{
    assert(m_Hwnd);
    ::ShowWindow(m_Hwnd, SW_SHOW);
}

void Window::Hide()
{
    assert(m_Hwnd);
    ::ShowWindow(m_Hwnd, SW_HIDE);
}

void Window::Destory()
{
    m_pGUI.reset();

    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        //Tell the game instance which the window will be destroyed.
        pGame->DestroyWindowMessage();
    }
    if (m_Hwnd)
    {
        ::DestroyWindow(m_Hwnd);
        m_Hwnd = nullptr;
    }
}

void Window::Update(const UpdateEventArgs& UpdateArgs)
{
    m_pGUI->UpdateGUI();

    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->Update(UpdateArgs);
    }
}

void Window::Render(const RenderEventArgs& RenderArgs)
{
    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->Render(RenderArgs);
    }
}

void Window::KeyPressed(const KeyEventArgs& KeyArgs)
{
    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->KeyPressed(KeyArgs);
    }
}

void Window::KeyReleased(const KeyEventArgs& KeyArgs)
{
    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->KeyReleased(KeyArgs);
    }
}

void Window::MouseMove(const MouseMotionEventArgs& MouseMotionArgs)
{
    const_cast<MouseMotionEventArgs&>(MouseMotionArgs).RelX = MouseMotionArgs.X - m_PreviousMouseX;
    const_cast<MouseMotionEventArgs&>(MouseMotionArgs).RelY = MouseMotionArgs.Y - m_PreviousMouseY;

    m_PreviousMouseX = MouseMotionArgs.X;
    m_PreviousMouseY = MouseMotionArgs.Y;

    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->MouseMove(MouseMotionArgs);
    }
}

void Window::MouseButtonPressed(const MouseButtonEventArgs& MouseButtonArgs)
{
    m_PreviousMouseX = MouseButtonArgs.X;
    m_PreviousMouseY = MouseButtonArgs.Y;

    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->MouseButtonPressed(MouseButtonArgs);
    }
}

void Window::MouseButtonReleased(const MouseButtonEventArgs& MouseButtonArgs)
{
    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->MouseButtonReleased(MouseButtonArgs);
    }
}

void Window::MouseWheel(const MouseWheelEventArgs& MouseWhellArgs)
{
    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->MouseWheel(MouseWhellArgs);
    }
}

void Window::OnResize(const ResizeEventArgs& ResizeArgs)
{
    // We just adjust backbuffer size when necessary.
    if (m_Width != ResizeArgs.Width || m_Height != ResizeArgs.Height)
    {
        m_Width = ResizeArgs.Width;
        m_Height = ResizeArgs.Height;
        // Before resize backbuffer,we must make sure no command is executing.
        Application::GetApp()->Flush();
        // Before resize swapchin, we must reset backbuffer resources.
        for (int i = 0; i < m_BackBufferCount; ++i)
        {
            ResourceStateTracker::RemoveGlobalResourceState(m_BackBufferTextures[i]->GetD3D12Resource().Get());
            m_BackBufferTextures[i]->Reset();
        }

        DXGI_SWAP_CHAIN_DESC1 swapchainDesc1;
        ThrowIfFailed(m_dxgiSwapChain->GetDesc1(&swapchainDesc1));

        ThrowIfFailed(m_dxgiSwapChain->ResizeBuffers(
            m_BackBufferCount,
            m_Width,
            m_Height,
            swapchainDesc1.Format,
            swapchainDesc1.Flags));

        UpdateRenderTargetView();
        // Refresh backbuffer index.
        m_CurrentBackBufferIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();
    }
    std::shared_ptr<Game> pGame;
    if (pGame = m_pGame.lock())
    {
        pGame->OnResize(ResizeArgs);
    }
}

void Window::PointerFromGame(std::shared_ptr<Game> pGame)
{
    m_pGame = pGame;
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
    assert(m_Hwnd && "Before Create SwapChain, Create Window Firstly");
    Microsoft::WRL::ComPtr<IDXGIFactory4> Factory4;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    UINT FactoryDebugFlag = 0;
#ifdef _DEBUG
    FactoryDebugFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(FactoryDebugFlag, IID_PPV_ARGS(&Factory4)));

    Microsoft::WRL::ComPtr<IDXGISwapChain1> SwapChain1;
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc;
    swapchainDesc.BufferCount = m_BackBufferCount;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc.Flags = m_SupportTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchainDesc.Width = m_Width;
    swapchainDesc.Height = m_Height;
    swapchainDesc.SampleDesc = { 1,0 };
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc.Stereo = FALSE;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    auto commandqueue = Application::GetApp()->GetCommandQueue()->GetD3D12CommandQueue().Get();

    ThrowIfFailed(Factory4->CreateSwapChainForHwnd(commandqueue, m_Hwnd, &swapchainDesc, 
        nullptr, nullptr, &SwapChain1));
    ThrowIfFailed(SwapChain1.As(&dxgiSwapChain4));
    //Disable ALT + ENTER to switch fullscreen.
    ThrowIfFailed(Factory4->MakeWindowAssociation(m_Hwnd, DXGI_MWA_NO_ALT_ENTER));
    //Since index may be not increased sequencely, we need to use method to get index.
    m_CurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

    return dxgiSwapChain4;
}

void Window::UpdateRenderTargetView()
{
    Microsoft::WRL::ComPtr<ID3D12Device2> device = Application::GetApp()->GetDevice();
    for (int i = 0; i < m_BackBufferCount; ++i)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> backbuffer;
        //GetBuffer() method will hele us create backbuffer resource.
        ThrowIfFailed(m_dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backbuffer)));
        //This is important!
        m_BackBufferTextures[i]->SetD3D12Resource(backbuffer);
        ResourceStateTracker::AddGlobalResourceState(m_BackBufferTextures[i]->GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_PRESENT);
        device->CreateRenderTargetView(m_BackBufferTextures[i]->GetD3D12Resource().Get(), nullptr, m_BackBufferTextures[i]->GetRenderTargetView());
    }
}

HWND Window::GetWindowHWND()const
{
    return m_Hwnd;
}

bool Window::IsFullScreen()const
{
    return m_FullScreenState;
}

void Window::SetFullScreenState(bool FullScreenState)
{
    assert(m_Hwnd && "Before switch screen state,create window firstly.");
    if (m_FullScreenState)
    {
        //before we switch to fullscreen state, we need to record current window info.
        ::GetWindowRect(m_Hwnd, &m_WindowRect);
        //reset the window style falg
        UINT windowFlag = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        ::SetWindowLongPtr(m_Hwnd, GWL_STYLE, windowFlag);
        //query nearest monitor screen and get monitor info
        HMONITOR hMonitor = ::MonitorFromWindow(m_Hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfor = {};
        monitorInfor.cbSize = sizeof(monitorInfor);
        ::GetMonitorInfo(hMonitor, &monitorInfor);
        //reset window pos
        ::SetWindowPos(m_Hwnd, HWND_NOTOPMOST,
            monitorInfor.rcMonitor.left, monitorInfor.rcMonitor.top,
            monitorInfor.rcMonitor.right - monitorInfor.rcMonitor.left,
            monitorInfor.rcMonitor.bottom - monitorInfor.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);
        //
        ::ShowWindow(m_Hwnd, SW_MAXIMIZE);
    }
    else//Windowed State
    {
        //set window style back original style
        UINT windowFlag = WS_OVERLAPPEDWINDOW;
        ::SetWindowLongPtr(m_Hwnd, GWL_STYLE, windowFlag);
        //set window pos from windowRect
        ::SetWindowPos(m_Hwnd, HWND_TOPMOST,
            m_WindowRect.left, m_WindowRect.top,
            m_WindowRect.right - m_WindowRect.left,
            m_WindowRect.bottom - m_WindowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);
        //
        ::ShowWindow(m_Hwnd, SW_NORMAL);
    }
}

void Window::ToggleFullScreenState()
{
    m_FullScreenState = !m_FullScreenState;
    SetFullScreenState(m_FullScreenState);
}

bool Window::IsVsync()const
{
    return m_Vsync;
}

void Window::SetVsyncState(bool VsyncState)
{
    m_Vsync = VsyncState;
}

void Window::ToggleVsyncState()
{
    m_Vsync = !m_Vsync;
    SetVsyncState(m_Vsync);
}

int Window::GetClientWidth()const
{
    return m_Width;
}

int Window::GetClientHeight()const
{
    return m_Height;
}

const std::wstring& Window::GetWindowName()const
{
    return m_WindowName;
}

UINT Window::Present(const Texture* pTexture)
{
    assert(pTexture && "We can not present a null texture!");
    auto commandQueue = Application::GetApp()->GetCommandQueue();
    auto commandList = commandQueue->GetCommandList();

    auto backbuffer = m_BackBufferTextures[m_CurrentBackBufferIndex].get();

    auto SampleDesc = pTexture->GetD3D12ResourceDesc().SampleDesc;

    if (SampleDesc.Count == 1)
    {
        commandList->CopyResource(backbuffer, pTexture);
    }
    else//for MSAA
    {
        commandList->ResolveSubResource(backbuffer, pTexture);
    }
    //Here we use back buffer as render target to blend with GUI.
    RenderTarget rendertarget;
    rendertarget.AttachTexture(Color0, *backbuffer);

    m_pGUI->RenderGUI(commandList, rendertarget);

    commandList->BarrierTransition(backbuffer, D3D12_RESOURCE_STATE_PRESENT);
    commandQueue->ExecuteCommandList(commandList);

    UINT SyncInterval = m_Vsync ? 1 : 0;
    //Notice:I do not know why the Flag - DXGI_PRESENT_ALLOW_TEARING will cause Present Error.
    //So I have to make sure the flag is always ZERO by forcing the m_Vsync is TRUE.
    UINT Flag = m_SupportTearing && !m_Vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(m_dxgiSwapChain->Present(SyncInterval, Flag));
    //After present,do not forget to refresh current backbuffer index.
    m_FenceValue[m_CurrentBackBufferIndex] = commandQueue->Signal();
    m_FrameCount[m_CurrentBackBufferIndex] = Application::GetFrameCount();

    m_CurrentBackBufferIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();

    commandQueue->WaitForFenceValue(m_FenceValue[m_CurrentBackBufferIndex]);
    //After all command compeleted,we can safely release all stale descriptors
    Application::GetApp()->ReleaseStaleDescriptors(m_FrameCount[m_CurrentBackBufferIndex]);

    return m_CurrentBackBufferIndex;
}

UINT Window::GetCurrentBackBufferIndex()const
{
    assert(m_dxgiSwapChain);
    return (UINT)m_CurrentBackBufferIndex;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView()const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_BackBufferTextures[m_CurrentBackBufferIndex]->GetRenderTargetView());
}

Texture* Window::GetCurrentBackBuffer()const
{
    assert(m_dxgiSwapChain);
    return m_BackBufferTextures[m_CurrentBackBufferIndex].get();
}

Window::~Window()
{
    assert(!m_Hwnd && "Use Application:DestoryWindow before destruction");
}



