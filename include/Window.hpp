#pragma once
#include <memory>
#include <SDL3/SDL.h>

struct SDLWindowDeleter {
    void operator()(SDL_Window* Ptr) const {
        if (Ptr && SDL_WasInit(SDL_INIT_VIDEO)) {
            SDL_DestroyWindow(Ptr);
        }
    }
};

using UniqueSDLWindow = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

class Window {
  public:
    Window();

    SDL_Window*   GetRaw() const;
    SDL_Surface*  GetSurface() const;
    SDL_Renderer* GetRenderer() const;

    void Render();
    void Update();
    int  GetWidth() const;
    int  GetHeight() const;
    void TakeScreenshot(std::string Location);

    Uint32 Red;
    Uint32 Green;
    Uint32 DarkGreen;
    Uint32 Blue;
    Uint32 Yellow;
    Uint32 Black;
    Uint32 Gray;

  private:
    UniqueSDLWindow SDLWindow{nullptr};
    SDL_Renderer*   mRenderer{nullptr};
};
