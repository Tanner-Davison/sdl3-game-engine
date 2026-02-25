#pragma once
#include <SDL3/SDL.h>
#include <iostream>

/// Define this to enable SDL error logging to stderr.
#define ERROR_LOGGING

/**
 * @brief Checks for a pending SDL error and logs it to stderr if present.
 *
 * When ERROR_LOGGING is defined, this function reads the current SDL error
 * string. If it's non-empty, it logs the provided action context and the
 * error message to stderr, then clears the SDL error buffer.
 *
 * If ERROR_LOGGING is not defined, this function is a no-op.
 *
 * @param Action A descriptive label for what operation was being attempted
 *               when the error may have occurred (e.g. "Creating Window").
 *
 * @par Example
 * @code
 * SDL_Window* win = SDL_CreateWindow(...);
 * CheckSDLError("Creating Window");
 * @endcode
 */
void CheckSDLError(const std::string& Action) {
#ifdef ERROR_LOGGING
    const char* Error(SDL_GetError());
    if (*Error != '\0') {
        std::cerr << Action << " Error:" << Error << "\n";
        SDL_ClearError();
    }
#endif
}
