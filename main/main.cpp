#include "Scenes.h"
#include <NeoEngine/inc/Application.h>
#include <NeoEngine/inc/d3dUtil.h>
#include <dxgidebug.h>

#pragma comment(lib,"dxguid.lib")

void ReportLiveObjects()
{
    IDXGIDebug1* dxgiDebug;
    ::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    dxgiDebug->Release();
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    try
    {
        int returnCode = 0;

        Application::Create(hInstance);
        {
            std::shared_ptr<Scenes> myScene = std::make_unique<Scenes>(L"Scenes", 1920, 1080, TRUE);
            returnCode = Application::GetApp()->Run(myScene);
        }
        Application::Destory();

        ::atexit(&ReportLiveObjects);

        return returnCode;
    }
    catch (const DxException& error)
    {
        MessageBox(NULL, error.ToString().c_str(), L"Error", MB_OK | MB_ICONERROR);
    }

}