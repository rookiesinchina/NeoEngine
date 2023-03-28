#pragma once

#include "DescriptorAllocation.h"
#include <memory>
#include <dxgi1_6.h>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>


#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"D3D12.lib")
#pragma comment(lib,"dxgi.lib")

//forward-declaration
class Window;
class Game;
class GameTimer;
class CommandQueue;
class DescriptorAllocator;

class Application
{
public:
    /**
     * Create an application
     */
    static void Create(HINSTANCE hinstance);
    /**
     * Get a static pointer of application to use member functions.
     */
    static Application* GetApp();
    /**
     * Destory an application
     */
    static void Destory();
    /**
     * Get current total frame of this app.
     * This function is used to release stale descriptors after relevant frame has been completed.
     * But here is a question is: Why we use frame to release stale descriptors instead of fencevalue?
     */
    static UINT64 GetFrameCount()
    {
        return m_FrameCount;
    }
    /**
     * Create descriptor heap according to type and size
     * This function is just for simple demo.
     * If you want some descriptors , for more efficient, you should use AllocateDescriptors() method.
     */
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type,UINT NumDescriptors);
    /**
     * Get descriptor size
     */
    UINT GetDescriptorIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE Type);
    /**
     * Use DescriptorAllocation class to allocate some descriptors from descriptor heap.
     */
    DescriptorAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT DescriptorsNum);
    /**
     * Flush all commandqueue to achieve sync between CPU and GPU
     */
    void Flush();
    /**
     * Get D3D12 Device
     */
    Microsoft::WRL::ComPtr<ID3D12Device2> GetDevice()const;
    /**
     * Get a struct commandqueue to record or execute commands and create swapchain.
     * We create three different command queue for possible usage.
     * D3D12_COMMAND_LIST_TYPE_COPY : a command queue only for copy command
     * D3D12_COMMAND_LIST_TYPE_COMPUTE : a command queue only for dispath command
     * D3D12_COMMAND_LIST_TYPE_DIRECT : a command queue for copy,dispath or draw command.
     */
    std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE Type = D3D12_COMMAND_LIST_TYPE_DIRECT);
    /**
     * Create rendering window for application
     */
    std::shared_ptr<Window> CreateRenderWindow(const std::wstring& windowName,int clientWidth,int clientHeight,bool vsync);
    /**
     * Get a window pointer from map  according to HWND
     */
    std::shared_ptr<Window> GetWindowPtr(HWND hwnd)const;
    /**
     * Get a window pointer from map according to Name
     */
    std::shared_ptr<Window> GetWindowPtr(std::wstring windowName)const;
    /**
     * Destory a window from name
     */
    void DestroyWindow(const std::wstring& windowName);
    /**
     * Destory a window from a window pointer
     */
    void DestroyWindow(std::shared_ptr<Window> pWindow);
    /**
     * Get support tearing state
     */
    bool GetTearingState()const;
    /**
     * Run application when everything is ready
     * @return message Wparam --int type
     */
    int Run(std::shared_ptr<Game> pGame);
    /**
     * Get application timer
     */
    std::shared_ptr<GameTimer> GetTimer()const;
    /**
     * Set app pasue state
     */
    void SetAppPausedState(bool State);
    /**
     * Get app state
     */
    bool GetAppPausedState()const;
    /**
     * Get upper-most MSAA state of current hardware
     */
    DXGI_SAMPLE_DESC CheckMultipleSampleQulityLevels(DXGI_FORMAT format, UINT numSamples, D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE)const;
    /**
     * Release all stale descriptor in descirptoralloctor
     * This function will be called after relevant frame has been completed in Window::Present()
     */
    void ReleaseStaleDescriptors(UINT64 frame);

    static UINT m_MultiSampleCount;
protected:
    Application(HINSTANCE hinstance);
    ~Application();
    /**
     * Get desired adapter to create d3d12 device.
     */
    Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter();
    /**
     * Use adapter to create a valid d3d12 device and enable debug info
     */
    Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
    /**
     * Check if hardware supports G-sync
     */
    bool CheckSupportTearing();
    /**
     * 
     */
    void Initialize();
private:
    friend LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HINSTANCE m_hinstance;
    bool m_bUseWarp;
    bool m_bSupportTearing;
    bool m_bIsAppPaused;

    std::shared_ptr<GameTimer> m_pTimer;
    static UINT64 m_FrameCount;

    Microsoft::WRL::ComPtr<ID3D12Device2> m_d3d12Device;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> m_dxgiAdapter;

    std::shared_ptr<CommandQueue> m_DirectCommandQueue;
    std::shared_ptr<CommandQueue> m_CopyCommandQueue;
    std::shared_ptr<CommandQueue> m_ComputeCommandQueue;

    std::unique_ptr<DescriptorAllocator> m_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
};