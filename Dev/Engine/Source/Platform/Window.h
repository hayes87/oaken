#pragma once

#include <string>
#include <SDL3/SDL.h>

namespace Platform {

    struct WindowProps {
        std::string Title;
        int Width;
        int Height;
    };

    class Window {
    public:
        Window(const WindowProps& props);
        ~Window();

        bool Init();
        void Shutdown();
        
        SDL_Window* GetNativeWindow() const { return m_Window; }
        int GetWidth() const { return m_Data.Width; }
        int GetHeight() const { return m_Data.Height; }
        float GetAspectRatio() const { return m_Data.Height > 0 ? (float)m_Data.Width / (float)m_Data.Height : 1.0f; }
        
        void OnResize(int width, int height) { m_Data.Width = width; m_Data.Height = height; }

        bool ShouldClose() const { return m_ShouldClose; }
        void SetShouldClose(bool close) { m_ShouldClose = close; }

    private:
        SDL_Window* m_Window = nullptr;
        WindowProps m_Data;
        bool m_ShouldClose = false;
    };

}
