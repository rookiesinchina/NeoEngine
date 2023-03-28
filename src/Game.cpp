#include "Game.h"
#include "Application.h"
#include "Window.h"

Game::Game(const std::wstring& gamename, int width, int height, bool Vsync)
{
    m_GameName = gamename;
    m_Width = width;
    m_Height = height;
    m_Vsync = Vsync;

}

Game::~Game()
{
}

bool Game::Initialize()
{
    //Check if support DirectX math library
    if (!DirectX::XMVerifyCPUSupport())
    {
        MessageBox(nullptr, L"CPU does not support DirectX", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    //Get a window which is associated with this app
    //Create a window , swapchain and associated DirectX12 contents.
    m_pWindow = Application::GetApp()->CreateRenderWindow(m_GameName, m_Width, m_Height,m_Vsync);
    m_pWindow->PointerFromGame(shared_from_this());
    m_pWindow->Show();

    return true;
}

std::shared_ptr<Window> Game::GetWindowPtr()const
{
    return m_pWindow;
}

void Game::DestroyWindowMessage()
{
    UnloadContent();
}

int Game::GetClientWidth()const
{
    return m_Width;
}

int Game::GetClientHeight()const
{
    return m_Height;
}

void Game::OnResize(const ResizeEventArgs& ResizeArgs)
{
    m_Width = ResizeArgs.Width;
    m_Height = ResizeArgs.Height;
}

void Game::Destroy()
{
    m_pWindow->Destory();
    m_pWindow.reset();
}