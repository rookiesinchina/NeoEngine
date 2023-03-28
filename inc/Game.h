#pragma once

#include "Events.h"

#include <memory>
#include <cstdint>
#include <string>
#include <DirectXMath.h>
#include <windows.h>

class Window;

class Game : public std::enable_shared_from_this<Game>
{
public:
    Game(const std::wstring& gamename, int width, int height, bool Vsync);
    virtual ~Game();

    int GetClientWidth()const;
    int GetClientHeight()const;

    /**
     * Initialize DirectX Runtime
     */
    virtual bool Initialize();
    /**
     * Load all contents of the demo
     */
    virtual bool LoadContent() = 0;
    /**
     * Unload all contents before destory
     */
    virtual bool UnloadContent() = 0;
    /**
     * Destory all resources
     */
    virtual void Destroy();
    /**
     * Get a window which is associated with this game
     */
    std::shared_ptr<Window> GetWindowPtr()const;
protected:
    // We need the class Window to visit some members of class Game.
    friend class Window;
    /**
     * Update rendering logic
     */
    virtual void Update(const UpdateEventArgs& UpdateArgs) {};
    /**
     * Draw rendering content
     */
    virtual void Render(const RenderEventArgs& RenderArgs) {};
    /**
     * Handle pressed keyboard
     */
    virtual void KeyPressed(const KeyEventArgs& KeyArgs) {};
    /**
     * Handle released keyboard
     */
    virtual void KeyReleased(const KeyEventArgs& KeyArgs) {};
    /**
     * Handle mouse move
     */
    virtual void MouseMove(const MouseMotionEventArgs& MouseMoveArgs) {};
    /**
     * Handle mouse button pressed
     */
    virtual void MouseButtonPressed(const MouseButtonEventArgs& MouseButtonArgs) {};
    /**
     * Handle mouse button released
     */
    virtual void MouseButtonReleased(const MouseButtonEventArgs& MouseButtonArgs) {};
    /**
     * Handle mouse wheel
     */
    virtual void MouseWheel(const MouseWheelEventArgs& MouseWheelArgs) {};
    /**
     * Handle window resizing,if we reload this virtual function we need to explicitly invoke this function
     */
    virtual void OnResize(const ResizeEventArgs& ResizeArgs);
    /**
     * A destroy window message to game
     */
    void DestroyWindowMessage();
    /**
     * 
     */
    std::shared_ptr<Window> m_pWindow;
    //
    int m_Width;
    int m_Height;
private:
    bool m_Vsync;
    std::wstring m_GameName;
};