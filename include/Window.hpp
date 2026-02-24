#pragma once
#include <memory>
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

/**
 * @brief Custom deleter for SDL_Window used with std::unique_ptr.
 *
 * Safely destroys an SDL_Window only if SDL video was initialized,
 * preventing crashes during shutdown ordering issues.
 */
struct SDLWindowDeleter {
    void operator()(SDL_Window* Ptr) const {
        if (Ptr && SDL_WasInit(SDL_INIT_VIDEO)) {
            SDL_DestroyWindow(Ptr);
        }
    }
};

/// Alias for a unique_ptr-managed SDL_Window with a safe custom deleter.
using UniqueSDLWindow = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

/**
 * @class Window
 * @brief RAII wrapper around an SDL_Window with common rendering utilities.
 *
 * Manages the lifetime of an SDL window and its surface. Provides helpers
 * for rendering, resizing, color mapping, and screenshotting. All SDL
 * cleanup is handled automatically via the UniqueSDLWindow deleter.
 *
 * @note The window is created at 1440x1080 and is resizable by default.
 *
 * @see SDLWindowDeleter
 */
class Window {
  public:
    /**
     * @brief Creates the SDL window and initializes common colors.
     *
     * Allocates a 1440x1080 resizable SDL window and pre-maps frequently
     * used colors (Red, Green, Blue, etc.) to the window's pixel format.
     *
     * @throws std::runtime_error if SDL_CreateWindow fails and ERROR_LOGGING is not defined.
     */
    Window();

    /**
     * @brief Returns the raw SDL_Window pointer (non-owning).
     * @return Pointer to the underlying SDL_Window, or nullptr if uninitialized.
     */
    SDL_Window* GetRaw() const;

    /**
     * @brief Returns the SDL surface associated with the window.
     * @return Pointer to the window's SDL_Surface, or nullptr if the window is invalid.
     */
    SDL_Surface* GetSurface() const;

    /**
     * @brief Clears the window surface to black.
     *
     * Should be called at the start of each frame before rendering anything else.
     */
    void Render();

    /**
     * @brief Presents the rendered surface to the screen.
     *
     * Calls SDL_UpdateWindowSurface to flush the current surface to the display.
     * Should be called at the end of each frame.
     */
    void Update();

    /**
     * @brief Returns the current width of the window in pixels.
     * @return Window width.
     */
    int GetWidth() const;

    /**
     * @brief Returns the current height of the window in pixels.
     * @return Window height.
     */
    int GetHeight() const;

    /**
     * @brief Saves the current window surface as a PNG file.
     * @param Location File path to write the screenshot (e.g. "screenshot.png").
     */
    void TakeScreenshot(std::string Location);

    SDL_PixelFormat* Fmt;       ///< Pixel format of the window surface.
    Uint32           Red;       ///< Pre-mapped red color value.
    Uint32           Green;     ///< Pre-mapped green color value.
    Uint32           DarkGreen; ///< Pre-mapped dark green color value.
    Uint32           Blue;      ///< Pre-mapped blue color value.
    Uint32           Yellow;    ///< Pre-mapped yellow color value.
    Uint32           Black;     ///< Pre-mapped black color value (near-black: RGB 0,0,1).
    Uint32           Gray;      ///< Pre-mapped gray color value.

  private:
    UniqueSDLWindow SDLWindow{nullptr}; ///< Owned SDL window handle.
};
