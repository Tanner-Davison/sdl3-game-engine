#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <string>

struct SDLWindowDeleter {
    void operator()(SDL_Window* Ptr) const {
        if (Ptr && SDL_WasInit(SDL_INIT_VIDEO)) {
            SDL_DestroyWindow(Ptr);
        }
    }
};

using UniqueSDLWindow = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

// Note: SDL_GetWindowSurface and SDL_CreateRenderer are mutually exclusive.
// This engine uses surface-based rendering — do not create a renderer on this window.
class Window {
  public:
    Window();

    SDL_Window*  GetRaw() const;
    SDL_Surface* GetSurface() const;

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
};
