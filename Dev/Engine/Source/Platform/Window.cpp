#include "Window.h"
#include <iostream>

namespace Platform {

    Window::Window(const WindowProps& props) : m_Data(props) {}

    Window::~Window() {
        Shutdown();
    }

    bool Window::Init() {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
            return false;
        }

        m_Window = SDL_CreateWindow(m_Data.Title.c_str(), m_Data.Width, m_Data.Height, SDL_WINDOW_RESIZABLE);
        if (!m_Window) {
            std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
            return false;
        }

        return true;
    }

    void Window::Shutdown() {
        if (m_Window) {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }
        SDL_Quit();
    }

}
