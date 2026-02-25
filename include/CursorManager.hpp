#pragma once
#include <SDL3/SDL.h>
class CursorManager {
  private:
    SDL_Cursor* defaultCursor;
    SDL_Cursor* GrabCursor;
    SDL_Cursor* HandCursor;
    SDL_Cursor* BlockedCursor;

  public:
    CursorManager() {
        defaultCursor = SDL_GetDefaultCursor();
        GrabCursor    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
        HandCursor    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
        BlockedCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
    }

    ~CursorManager() {
        SDL_DestroyCursor(defaultCursor);
        SDL_DestroyCursor(GrabCursor);
        SDL_DestroyCursor(HandCursor);
        SDL_DestroyCursor(BlockedCursor);
    }

    void setGrabCursor() {
        SDL_SetCursor(GrabCursor);
    }

    void setDefaultCursor() {
        SDL_SetCursor(defaultCursor);
    }
    void setHandCursor() {
        SDL_SetCursor(HandCursor);
    }

    void setBlockedCursor() {
        SDL_SetCursor(BlockedCursor);
    }
};
