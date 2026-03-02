#include "Window.hpp"
#include "ErrorHandling.hpp"
#include <SDL3_image/SDL_image.h>
#include <stdexcept>

Window::Window() {
    SDL_Window* Ptr = SDL_CreateWindow("SDL3 Sandbox", 1440, 1080,
                                       SDL_WINDOW_RESIZABLE);
    CheckSDLError("Creating Window");

    if (!Ptr) {
        throw std::runtime_error(std::string("Failed to create Window: ") +
                                 SDL_GetError());
    }

    SDLWindow.reset(Ptr);

    // Note: SDL3 enables drop file events by default — no opt-in call needed.
    // Note: do not call SDL_CreateRenderer on this window.
    // SDL_GetWindowSurface and SDL_CreateRenderer are mutually exclusive on Mac.
    // This engine uses surface-based rendering throughout.

    SDL_Surface* surf = GetSurface();
    if (surf) {
        const SDL_PixelFormatDetails* details =
            SDL_GetPixelFormatDetails(surf->format);
        DarkGreen = SDL_MapRGB(details, nullptr, 0, 150, 100);
        Yellow    = SDL_MapRGB(details, nullptr, 255, 255, 0);
        Green     = SDL_MapRGB(details, nullptr, 0, 255, 0);
        Red       = SDL_MapRGB(details, nullptr, 255, 0, 0);
        Blue      = SDL_MapRGB(details, nullptr, 0, 0, 255);
        Black     = SDL_MapRGB(details, nullptr, 0, 0, 1);
        Gray      = SDL_MapRGB(details, nullptr, 134, 149, 149);
    }
}

SDL_Window* Window::GetRaw() const {
    return SDLWindow.get();
}

SDL_Surface* Window::GetSurface() const {
    return SDLWindow ? SDL_GetWindowSurface(SDLWindow.get()) : nullptr;
}

void Window::Render() {
    SDL_Surface* surf = GetSurface();
    if (surf) {
        const SDL_PixelFormatDetails* details =
            SDL_GetPixelFormatDetails(surf->format);
        SDL_FillSurfaceRect(surf, nullptr,
                            SDL_MapRGB(details, nullptr, 0, 0, 1));
    }
}

void Window::Update() {
    SDL_UpdateWindowSurface(SDLWindow.get());
}

int Window::GetWidth() const {
    int w;
    SDL_GetWindowSize(SDLWindow.get(), &w, nullptr);
    return w;
}

int Window::GetHeight() const {
    int h;
    SDL_GetWindowSize(SDLWindow.get(), nullptr, &h);
    return h;
}

void Window::TakeScreenshot(std::string Location) {
    IMG_SavePNG(GetSurface(), Location.c_str());
}
