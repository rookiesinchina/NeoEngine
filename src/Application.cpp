#include "Application.h"
#include "Window.h"
#include "CommandQueue.h"
#include "Game.h"
#include "GameTimer.h"
#include "d3dUtil.h"
#include "DescriptorAllocator.h"
#include "imgui_impl_win32.h"

const std::wstring g_WindowClassName = L"DirectX12";

using WindowPtr = std::shared_ptr<Window>;
using WindowMap = std::unordered_map<HWND, WindowPtr>;
using WindowNameMap = std::unordered_map<std::wstring, WindowPtr>;
/**
 * Attention!!We must clear these global static variables since they save some intelligent pointers.
 * And we can not wrongly think that the Application or other class can help us destroy these variables.
 * Because these variables are global, the variables are destroyed after these classes destroy!!
 */
static WindowMap gs_Windows;
static WindowNameMap gs_WindowsByName;
static Application* m_SingleApp = nullptr;
static bool gb_IsDxRuntimeReady = false;

UINT Application::m_MultiSampleCount = 4;

//We should set window callback function in Application since callback function will
//frequently use global variables.
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
//
UINT64 Application::m_FrameCount = 0;

/**
 * A helper struct to create window.
 */
struct MakeWindow : public Window
{
	MakeWindow(HWND hwnd, const std::wstring& windowName, int width, int height, bool vsync)
		:Window(hwnd, windowName, width, height, vsync) {};
};

Application::Application(HINSTANCE hinstance) : 
    m_hinstance(hinstance),
    m_bUseWarp(FALSE),
    m_bIsAppPaused(FALSE),
    m_bSupportTearing(FALSE)
{
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	//Enable d3d12 debug layer
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif

    WNDCLASSEXW wndclassexw = {};
    wndclassexw.cbSize = sizeof(wndclassexw);
    wndclassexw.style = CS_HREDRAW | CS_VREDRAW;
    wndclassexw.lpfnWndProc = &WndProc;
    wndclassexw.cbClsExtra = 0;
    wndclassexw.cbWndExtra = 0;
    wndclassexw.hInstance = m_hinstance;
    wndclassexw.hIcon = LoadIcon(m_hinstance, IDI_APPLICATION);
    wndclassexw.hIconSm = LoadIcon(m_hinstance, IDI_APPLICATION);
    wndclassexw.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndclassexw.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wndclassexw.lpszClassName = g_WindowClassName.c_str();
    wndclassexw.lpszMenuName = 0;

    if (!::RegisterClassExW(&wndclassexw))
    {
        ::MessageBox(nullptr, L"Register Window Failed", L"ERROR", MB_OK | MB_ICONERROR);
    }
}

void Application::Initialize()
{
    m_dxgiAdapter = GetAdapter();
    if (m_dxgiAdapter)
    {
        m_d3d12Device = CreateDevice(m_dxgiAdapter);
    }
    if (m_d3d12Device)
    {
        m_DirectCommandQueue = std::make_shared<CommandQueue>(D3D12_COMMAND_LIST_TYPE_DIRECT);
        m_ComputeCommandQueue = std::make_shared<CommandQueue>(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        m_CopyCommandQueue = std::make_shared<CommandQueue>(D3D12_COMMAND_LIST_TYPE_COPY);

        m_bSupportTearing = CheckSupportTearing();
        m_pTimer = std::make_shared<GameTimer>();
    }

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_DescriptorAllocator[i] = std::make_unique<DescriptorAllocator>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
    }
}

Application::~Application()
{
	Flush();
}

void Application::Create(HINSTANCE hinstance)
{
	if (m_SingleApp == nullptr)
	{
		m_SingleApp = new Application(hinstance);
        m_SingleApp->Initialize();
	}
}

void Application::Destory()
{
	if (m_SingleApp)
	{
		/**
		 * Point 1.
		 */
		assert(gs_Windows.empty() && gs_WindowsByName.empty() &&
			"All windows should be destroyed before destroy the application instance");
		delete m_SingleApp;
		m_SingleApp = nullptr;
	}
}

Application* Application::GetApp()
{
	assert(m_SingleApp && "Uninitialized Application");
	return m_SingleApp;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Application::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type,UINT NumDescriptors)
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = NumDescriptors;
	heapDesc.Type = Type;

	ThrowIfFailed(m_d3d12Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));
	return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Device2> Application::GetDevice()const
{
	return m_d3d12Device;
}

UINT Application::GetDescriptorIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	return m_d3d12Device->GetDescriptorHandleIncrementSize(Type);
}

DescriptorAllocation Application::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT DescriptorsNum)
{
    assert(m_DescriptorAllocator[Type]);
    auto allocation = m_DescriptorAllocator[Type]->Allocate(DescriptorsNum);

    return allocation;
}

void Application::Flush()
{
	m_DirectCommandQueue->Flush();
	m_ComputeCommandQueue->Flush();
	m_CopyCommandQueue->Flush();
}

std::shared_ptr<CommandQueue> Application::GetCommandQueue(D3D12_COMMAND_LIST_TYPE Type /* = D3D12_COMMAND_LIST_TYPE_DIRECT */)
{
	std::shared_ptr<CommandQueue> commandqueue;
	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		commandqueue = m_DirectCommandQueue;
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		commandqueue = m_ComputeCommandQueue;
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		commandqueue = m_CopyCommandQueue;
		break;
	default:
		assert(false && "Invalid CommandQueue Type");
		break;
	}
	return commandqueue;
}

Microsoft::WRL::ComPtr<IDXGIAdapter4> Application::GetAdapter()
{
	UINT createFactoryFlag = 0;
#ifdef _DEBUG
	createFactoryFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif
	Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;
	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlag, IID_PPV_ARGS(&factory4)));

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter1;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter4;

	if (m_bUseWarp)
	{
		ThrowIfFailed(factory4->EnumWarpAdapter(IID_PPV_ARGS(&adapter4)));
	}
	else
	{
		UINT maxVideoMemorySize = 0;
        //Attention here!!! the Comptr.GetAddressof() and operator& have HUGE difference!!
        //GetAddressof() will get reference of interface without releasing the interface.
        //Operator& will release the interface firstly and then retrieve the address of the ComPtr object,
        //this is equal with ReleasingAndGetAddressof() method.
		for (UINT i = 0; factory4->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 adapterDesc;
			ThrowIfFailed(adapter1->GetDesc1(&adapterDesc));

			if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0	//pass software adapter	
				&& SUCCEEDED(D3D12CreateDevice(adapter1.Get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),nullptr)) // pass adapters which can not create d3d12 device
				&& maxVideoMemorySize < adapterDesc.DedicatedVideoMemory)//pass adapters which have less memory
			{
				maxVideoMemorySize = adapterDesc.DedicatedVideoMemory;
				ThrowIfFailed(adapter1.As(&adapter4));
			}
		}
	}

	return adapter4;
}

Microsoft::WRL::ComPtr<ID3D12Device2> Application::CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter)
{
	Microsoft::WRL::ComPtr<ID3D12Device2> device;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
#ifdef _DEBUG
	//set debug info 
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
	
	if (SUCCEEDED(device.As(&infoQueue)))
	{
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);


		//filter some debug messages
		D3D12_MESSAGE_SEVERITY severity[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};
		D3D12_MESSAGE_ID IDs[] =
		{
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_DEPTHSTENCILVIEW_NOT_SET
		};

		D3D12_INFO_QUEUE_FILTER newFilter = {};
		newFilter.DenyList.NumSeverities = _countof(severity);
		newFilter.DenyList.pSeverityList = severity;
		newFilter.DenyList.NumIDs = _countof(IDs);
		newFilter.DenyList.pIDList = IDs;

		ThrowIfFailed(infoQueue->PushStorageFilter(&newFilter));
	}
#endif
	return device;
}

bool Application::CheckSupportTearing()
{
	BOOL Support = false;

	Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
		if(SUCCEEDED(factory4.As(&factory5)))
		{
			factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &Support, sizeof(Support));
		}
	}

	return Support == TRUE;
}

std::shared_ptr<Window> Application::CreateRenderWindow(const std::wstring& windowName, int clientWidth, int clientHeight,bool vsync)
{
	//Check if the window has been created.
	WindowNameMap::iterator pos = gs_WindowsByName.find(windowName);
	if (pos != gs_WindowsByName.end())
	{
		return pos->second;
	}

    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int scrrenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    RECT rect = { 0,0,clientWidth,clientHeight };
    ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

    float width = rect.right - rect.left;
    float height = rect.bottom - rect.top;

    int windowX = (int)(0.5f * (screenWidth - width));
    int windowY = (int)(0.5f * (scrrenHeight - height));

    HWND pHwnd = CreateWindowW(g_WindowClassName.c_str(), windowName.c_str(), WS_OVERLAPPEDWINDOW, windowX, windowY, width, height,
        NULL, NULL, m_hinstance, NULL);
    if (!pHwnd)
    {
        MessageBox(nullptr, L"Create Window Failed", L"ERROR", MB_OK | MB_ICONERROR);
        return nullptr;
    }

	WindowPtr p_Window = std::make_shared<MakeWindow>(pHwnd, windowName, clientWidth, clientHeight, vsync);
	//insert new window into map
	gs_Windows.insert(WindowMap::value_type(pHwnd, p_Window));
	gs_WindowsByName.insert(WindowNameMap::value_type(windowName, p_Window));

	return p_Window;
}

std::shared_ptr<Window> Application::GetWindowPtr(HWND hwnd)const
{
	std::shared_ptr<Window> pWindow = nullptr;
	WindowMap::iterator pos = gs_Windows.find(hwnd);
	if (pos != gs_Windows.end())
	{
		pWindow = pos->second;
	}
	return pWindow;
}

std::shared_ptr<Window> Application::GetWindowPtr(std::wstring windowName)const
{
    std::shared_ptr<Window> pWindow = nullptr;
    WindowNameMap::iterator pos = gs_WindowsByName.find(windowName);
    if (pos != gs_WindowsByName.end())
    {
        pWindow = pos->second;
    }
    return pWindow;
}

void Application::DestroyWindow(const std::wstring& windowName)
{
	auto pWindow = GetWindowPtr(windowName);
	DestroyWindow(pWindow);
}

void Application::DestroyWindow(std::shared_ptr<Window> pWindow)
{
	if (pWindow) pWindow->Destory();
}

bool Application::GetTearingState()const
{
	return m_bSupportTearing;
}

int Application::Run(std::shared_ptr<Game> pGame)
{
	if (!pGame->Initialize())return 1;
	if (!pGame->LoadContent())return 2;

    gb_IsDxRuntimeReady = true;

	m_pTimer->Reset();

	MSG msg = {  };
	// Point 3 : Put update and render to message loop
	while (msg.message!=WM_QUIT)
	{
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	//We start to destroy all resources from here
	//And before destory,we must make sure all commands have been executed.
	Flush();
	pGame->Destroy();

	return (int)msg.wParam;
}

std::shared_ptr<GameTimer> Application::GetTimer()const
{
	return m_pTimer;
}

void Application::SetAppPausedState(bool State)
{
	m_bIsAppPaused = State;
}

bool Application::GetAppPausedState()const
{
	return m_bIsAppPaused;
}

static void RemoveWindow(HWND hwnd)
{
    WindowMap::iterator iter = gs_Windows.find(hwnd);
    if (iter != gs_Windows.end())
    {
        auto pWindow = iter->second;
        auto windowName = pWindow->GetWindowName();

        gs_Windows.erase(hwnd);
        gs_WindowsByName.erase(windowName);
    }
}

static void CalculateFrameStat(HWND hwnd)
{
    auto pTimer = Application::GetApp()->GetTimer();
    static int framecount = 0;
    static double previousTime = 0.0;

    if (pTimer->TotalTime() - previousTime >= 1.0)
    {
        auto windowName = Application::GetApp()->GetWindowPtr(hwnd)->GetWindowName();
        std::wstring captionText = windowName + L" FPS: " + std::to_wstring(framecount);

        ::SetWindowText(hwnd, captionText.c_str());

        framecount = 0;
        previousTime = pTimer->TotalTime();
    }
    ++framecount;
}

/************************************************************************/
/* Static Windows Callback Function                                     */
/************************************************************************/
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
        return true;


    std::shared_ptr<Window> pWindow = Application::GetApp()->GetWindowPtr(hwnd);
    std::shared_ptr<GameTimer> pGameTimer = Application::GetApp()->GetTimer();
    if (pWindow)
    {
        switch (message)
        {
        case WM_PAINT:
        {
            pGameTimer->Tick();
            if (!Application::GetApp()->GetAppPausedState() && gb_IsDxRuntimeReady)
            {
                ++Application::m_FrameCount;
                //Show FPS
                CalculateFrameStat(hwnd);
                //Then Render
                UpdateEventArgs UpdateArgs(pGameTimer->DeltaTime(), pGameTimer->TotalTime());
                pWindow->Update(UpdateArgs);
                RenderEventArgs RenderArgs(pGameTimer->DeltaTime(), pGameTimer->TotalTime());
                pWindow->Render(RenderArgs);
            }
        }
        break;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            MSG charMsg;
            // Get the Unicode character (UTF-16)
            unsigned int c = 0;
            // For printable characters, the next message will be WM_CHAR.
            // This message contains the character code we need to send the KeyPressed event.
            // Inspired by the SDL 1.2 implementation.
            if (PeekMessage(&charMsg, hwnd, 0, 0, PM_NOREMOVE) && charMsg.message == WM_CHAR)
            {
                GetMessage(&charMsg, hwnd, 0, 0);
                c = static_cast<unsigned int>(charMsg.wParam);
            }
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            KeyCode::Key key = (KeyCode::Key)wParam;
            unsigned int scanCode = (lParam & 0x00FF0000) >> 16;
            KeyEventArgs keyEventArgs(key, c, KeyEventArgs::Pressed, shift, control, alt);

            pWindow->KeyPressed(keyEventArgs);
        }
        break;
        case WM_SYSKEYUP:
        case WM_KEYUP:
        {
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            KeyCode::Key key = (KeyCode::Key)wParam;
            unsigned int c = 0;
            unsigned int scanCode = (lParam & 0x00FF0000) >> 16;

            // Determine which key was released by converting the key code and the scan code
            // to a printable character (if possible).
            // Inspired by the SDL 1.2 implementation.
            unsigned char keyboardState[256];
            GetKeyboardState(keyboardState);
            wchar_t translatedCharacters[4];
            if (int result = ToUnicodeEx(static_cast<UINT>(wParam), scanCode, keyboardState, translatedCharacters, 4, 0, NULL) > 0)
            {
                c = translatedCharacters[0];
            }

            KeyEventArgs keyEventArgs(key, c, KeyEventArgs::Released, shift, control, alt);
            pWindow->KeyReleased(keyEventArgs);
        }
        break;
        // The default window procedure will play a system notification sound 
        // when pressing the Alt+Enter keyboard combination if this message is 
        // not handled.
        case WM_SYSCHAR:
            break;
        case WM_MOUSEMOVE:
        {
            bool lButton = (wParam & MK_LBUTTON) != 0;
            bool rButton = (wParam & MK_RBUTTON) != 0;
            bool mButton = (wParam & MK_MBUTTON) != 0;
            bool shift = (wParam & MK_SHIFT) != 0;
            bool control = (wParam & MK_CONTROL) != 0;

            int x = ((int)(short)LOWORD(lParam));
            int y = ((int)(short)HIWORD(lParam));

            MouseMotionEventArgs mouseMotionEventArgs(lButton, mButton, rButton, control, shift, x, y);
            pWindow->MouseMove(mouseMotionEventArgs);
        }
        break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        {
            bool lButton = (wParam & MK_LBUTTON) != 0;
            bool rButton = (wParam & MK_RBUTTON) != 0;
            bool mButton = (wParam & MK_MBUTTON) != 0;
            bool shift = (wParam & MK_SHIFT) != 0;
            bool control = (wParam & MK_CONTROL) != 0;

            int x = ((int)(short)LOWORD(lParam));
            int y = ((int)(short)HIWORD(lParam));

            MouseButtonEventArgs mouseButtonEventArgs(DecodeMouseButton(message), MouseButtonEventArgs::Pressed, lButton, mButton, rButton, control, shift, x, y);
            pWindow->MouseButtonPressed(mouseButtonEventArgs);
        }
        break;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        {
            bool lButton = (wParam & MK_LBUTTON) != 0;
            bool rButton = (wParam & MK_RBUTTON) != 0;
            bool mButton = (wParam & MK_MBUTTON) != 0;
            bool shift = (wParam & MK_SHIFT) != 0;
            bool control = (wParam & MK_CONTROL) != 0;

            int x = ((int)(short)LOWORD(lParam));
            int y = ((int)(short)HIWORD(lParam));

            MouseButtonEventArgs mouseButtonEventArgs(DecodeMouseButton(message), MouseButtonEventArgs::Released, lButton, mButton, rButton, control, shift, x, y);
            pWindow->MouseButtonReleased(mouseButtonEventArgs);
        }
        break;
        case WM_MOUSEWHEEL:
        {
            // The distance the mouse wheel is rotated.
            // A positive value indicates the wheel was rotated to the right.
            // A negative value indicates the wheel was rotated to the left.
            float zDelta = ((int)(short)HIWORD(wParam)) / (float)WHEEL_DELTA;
            short keyStates = (short)LOWORD(wParam);

            bool lButton = (keyStates & MK_LBUTTON) != 0;
            bool rButton = (keyStates & MK_RBUTTON) != 0;
            bool mButton = (keyStates & MK_MBUTTON) != 0;
            bool shift = (keyStates & MK_SHIFT) != 0;
            bool control = (keyStates & MK_CONTROL) != 0;

            int x = ((int)(short)LOWORD(lParam));
            int y = ((int)(short)HIWORD(lParam));

            // Convert the screen coordinates to client coordinates.
            POINT clientToScreenPoint;
            clientToScreenPoint.x = x;
            clientToScreenPoint.y = y;
            ScreenToClient(hwnd, &clientToScreenPoint);

            MouseWheelEventArgs mouseWheelEventArgs(zDelta, lButton, mButton, rButton, control, shift, (int)clientToScreenPoint.x, (int)clientToScreenPoint.y);
            pWindow->MouseWheel(mouseWheelEventArgs);
        }
        break;
        case WM_SIZE:
        {
            int width = ((int)(short)LOWORD(lParam));
            int height = ((int)(short)HIWORD(lParam));
            if (width != 0 && height != 0)
            {
                ResizeEventArgs resizeEventArgs(width, height);
                pWindow->OnResize(resizeEventArgs);
            }
        }
        break;
        case WM_ENTERSIZEMOVE:
            Application::GetApp()->SetAppPausedState(TRUE);
            Application::GetApp()->GetTimer()->Stop();
            break;
        case WM_EXITSIZEMOVE:
            Application::GetApp()->SetAppPausedState(FALSE);
            Application::GetApp()->GetTimer()->Start();
            break;
        case WM_GETMINMAXINFO:
            ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
            ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
            break;
        case WM_DESTROY:
            RemoveWindow(hwnd);
            if (gs_Windows.empty() && gs_WindowsByName.empty())
            {
                PostQuitMessage(0);
            }
            break;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }
    else
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return 0;
}

DXGI_SAMPLE_DESC Application::CheckMultipleSampleQulityLevels(DXGI_FORMAT format, UINT numSamples, D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags)const
{
    DXGI_SAMPLE_DESC sampleDesc = { 1, 0 };

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
    qualityLevels.Format = format;
    qualityLevels.SampleCount = 1;
    qualityLevels.Flags = flags;
    qualityLevels.NumQualityLevels = 0;

    while (qualityLevels.SampleCount <= numSamples && SUCCEEDED(m_d3d12Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualityLevels, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS))) && qualityLevels.NumQualityLevels > 0)
    {
        // That works...
        sampleDesc.Count = qualityLevels.SampleCount;
        sampleDesc.Quality = qualityLevels.NumQualityLevels - 1;

        // But can we do better?
        qualityLevels.SampleCount *= 2;
    }

    return sampleDesc;
}

void Application::ReleaseStaleDescriptors(UINT64 frame)
{
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_DescriptorAllocator[i]->ReleaseStaleDescriptorAlloction(frame);
    }
}