#pragma once

#include <NeoEngine/inc/Game.h>
#include <NeoEngine/inc/Scene.h>
#include <NeoEngine/inc/Texture.h>
#include <NeoEngine/inc/Environment.h>
#include <NeoEngine/inc/Rendering.h>
#include <wrl.h>
#include <d3d12.h>

#include <string>
#include <memory>


#ifdef _DEBUG
#pragma comment(lib,"assimp-vc142-mtd.lib")
#else
#pragma comment(lib,"assimp-vc142-mt.lib")
#endif


class Camera;
class Scene;
class RenderTarget;
class Filter;

class Scenes : public Game
{
public:
    Scenes(const std::wstring& GameName, int Width, int Height, bool vsync);
    ~Scenes();

    bool LoadContent()override;

    bool UnloadContent()override;

    void Update(const UpdateEventArgs& UpdateArgs)override;

    void Render(const RenderEventArgs& RenderArgs)override;

    void OnResize(const ResizeEventArgs& ResizeArgs)override;

    void KeyPressed(const KeyEventArgs& KeyArgs)override;

    void KeyReleased(const KeyEventArgs& KeyArgs)override;

    void MouseButtonPressed(const MouseButtonEventArgs& MouseButtonArgs)override;

    void MouseButtonReleased(const MouseButtonEventArgs& MouseButtonArgs)override;

    void MouseMove(const MouseMotionEventArgs& MouseMoveArgs)override;

    void MouseWheel(const MouseWheelEventArgs& MouseWheelArgs)override {};
private:
    std::unique_ptr<Camera> m_pCamera;
    std::unique_ptr<Camera> m_pCamera2;

    std::unique_ptr<EnvironmentPassConstant> m_pEnvironmentPassConstant;

    std::shared_ptr<RenderTarget> m_pRenderTarget;

    float m_AspectRadio;

    //Some control parameter
    float m_CameraWalk = 0.0f;
    float m_CameraStrife = 0.0f;
    float m_CameraPitch = 1.0f;
    float m_CameraHead = 1.0f;

    Light* plight;

    std::unique_ptr<ForwardRendering> m_pForwardRendering;
};