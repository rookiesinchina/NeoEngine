#pragma once


#include "Texture.h"
#include "Events.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_5.h>
#include <string>
#include <cassert>
#include <memory>



class Game;
class GUI;

class Window
{
public:
    const static int m_BackBufferCount = 3;
    /**
     * Show window
     */
    void Show();
    /**
     * Hide window
     */
    void Hide();
    /**
     * Destory window
     */
    void Destory();

    /**
     * Get window handle
     */
    HWND GetWindowHWND()const;
    /**
     * Handle window fullscreen state
     */
    bool IsFullScreen()const;
    void SetFullScreenState(bool FullScreenState);
    void ToggleFullScreenState();
    /**
     * Handle swapchain v_sync state
     */
    bool IsVsync()const;
    void SetVsyncState(bool VsyncState);
    void ToggleVsyncState();
    /**
     * Get window size and name
     */
    int GetClientWidth()const;
    int GetClientHeight()const;
    const std::wstring& GetWindowName()const;
    /**
     * We copy render target color0 texture to backbuffer
     * Present backbuffer content
     * @return the current backbuffer index
     */
    UINT Present(const Texture* pTexture);
    /**
     * Get current backbuffer index
     */
    UINT GetCurrentBackBufferIndex()const;
    /**
     * Get current backbuffer render target view
     */
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView()const;
    /**
     * Get current backbuffer resource
     */
    Texture* GetCurrentBackBuffer()const;
protected:
    /**
     * We need to application class to visit privated or protected members
     */
    friend class Application;
    /**
     * We need to game class to visit privated or protected members
     */
    friend class Game;
    /**
     * We need WndPrco funtion to visit privated or protected members
     */
    friend LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    /**
     * Get pointer from class game
     */
    void PointerFromGame(std::shared_ptr<Game> pGame);
    /**
     * Create a swapchain which is associated with hwnd.
     */
    Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain();
    /**
     * Get backbuffers for swapchain and create/recreate render target view.
     */
    void UpdateRenderTargetView();
    /**
     * Update
     */
    void Update(const UpdateEventArgs& UpdateArgs);
    /**
     * Render
     */
    void Render(const RenderEventArgs& RenderArgs);
    /**
     * Respond keyboard pressed
     */
    void KeyPressed(const KeyEventArgs& KeyArgs);
    /**
     * Respond keyboard released
     */
    void KeyReleased(const KeyEventArgs& KeyArgs);
    /**
     * Respond mouse move
     */
    void MouseMove(const MouseMotionEventArgs& MouseMotionArgs);
    /**
     * Respond mouse button pressed
     */
    void MouseButtonPressed(const MouseButtonEventArgs& MouseButtonArgs);
    /**
     * Respond mouse button released
     */
    void MouseButtonReleased(const MouseButtonEventArgs& MouseButtonArgs);
    /**
     * Respond mouse wheel
     */
    void MouseWheel(const MouseWheelEventArgs& MouseWhellArgs);
    /**
     * Respond resize window
     */
    void OnResize(const ResizeEventArgs& ResizeArgs);
    /**
    * Construction function and put this function into pretected members since we just want to create the window instance
    * in other class
    */
    Window(HWND hwnd, const std::wstring& windowName, int width, int height, bool vsync);
    virtual ~Window();
private:
    HWND m_Hwnd;
    std::wstring m_WindowName;

    int m_Width;
    int m_Height;
    bool m_Vsync;
    bool m_SupportTearing;
    bool m_FullScreenState;

    RECT m_WindowRect;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_dxgiSwapChain;
    std::unique_ptr<Texture> m_BackBufferTextures[m_BackBufferCount];

    UINT64 m_FenceValue[m_BackBufferCount];
    UINT64 m_FrameCount[m_BackBufferCount];

    int m_CurrentBackBufferIndex;

    std::weak_ptr<Game> m_pGame;
    //Mouse postion info
    float m_PreviousMouseX;
    float m_PreviousMouseY;
    //
    std::unique_ptr<GUI> m_pGUI;
};