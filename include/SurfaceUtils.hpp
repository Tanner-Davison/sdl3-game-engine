#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>

/// Rotates an SDL_Surface 90 degrees clockwise.
/// Returns a new surface — caller is responsible for freeing it.
inline SDL_Surface* RotateSurface90CW(SDL_Surface* src) {
    SDL_Surface* dst = SDL_CreateSurface(src->h, src->w, src->format);
    SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            Uint32* srcPx = (Uint32*)((Uint8*)src->pixels + y * src->pitch + x * 4);
            Uint32* dstPx =
                (Uint32*)((Uint8*)dst->pixels + x * dst->pitch + (src->h - 1 - y) * 4);
            *dstPx = *srcPx;
        }
    }
    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dst);
    return dst;
}

/// Returns the x position needed to horizontally center text within a container.
/// @param font        The TTF font to measure with
/// @param text        The string to measure
/// @param containerX  Left edge of the container (e.g. a button's x)
/// @param containerW  Width of the container (e.g. a button's width)
inline int CenterTextX(TTF_Font*          font,
                       const std::string& text,
                       int                containerX,
                       int                containerW) {
    int w = 0;
    TTF_GetStringSize(font, text.c_str(), 0, &w, nullptr);
    return containerX + (containerW - w) / 2;
}

/// Returns the y position needed to vertically center text within a container.
/// @param font        The TTF font to measure with
/// @param containerY  Top edge of the container
/// @param containerH  Height of the container
inline int CenterTextY(TTF_Font* font, int containerY, int containerH) {
    int h = 0;
    TTF_GetStringSize(font, "A", 0, nullptr, &h); // height is same for all chars
    return containerY + (containerH - h) / 2;
}

/// Returns both x and y to center text within a rectangle.
inline SDL_Point CenterTextInRect(TTF_Font*          font,
                                  const std::string& text,
                                  const SDL_Rect&    rect) {
    int w = 0, h = 0;
    TTF_GetStringSize(font, text.c_str(), 0, &w, &h);
    return {rect.x + (rect.w - w) / 2, rect.y + (rect.h - h) / 2};
}

/// Returns an SDL_Rect centered within a container rect.
/// @param containerRect  The rect to center within
/// @param w              Width of the rect to center
/// @param h              Height of the rect to center
/// @param offsetY        Optional vertical offset from center (positive = down)
inline SDL_Rect CenterRect(const SDL_Rect& container, int w, int h, int offsetY = 0) {
    return {container.x + (container.w - w) / 2,
            container.y + (container.h - h) / 2 + offsetY,
            w,
            h};
}

/// Rotates an SDL_Surface 90 degrees counter-clockwise.
/// Returns a new surface — caller is responsible for freeing it.
inline SDL_Surface* RotateSurface90CCW(SDL_Surface* src) {
    SDL_Surface* dst = SDL_CreateSurface(src->h, src->w, src->format);
    SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            Uint32* srcPx = (Uint32*)((Uint8*)src->pixels + y * src->pitch + x * 4);
            Uint32* dstPx =
                (Uint32*)((Uint8*)dst->pixels + (src->w - 1 - x) * dst->pitch + y * 4);
            *dstPx = *srcPx;
        }
    }
    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dst);
    return dst;
}

/// Rotates an SDL_Surface 180 degrees.
/// Returns a new surface — caller is responsible for freeing it.
inline SDL_Surface* RotateSurface180(SDL_Surface* src) {
    SDL_Surface* dst = SDL_CreateSurface(src->w, src->h, src->format);
    SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            Uint32* srcPx = (Uint32*)((Uint8*)src->pixels + y * src->pitch + x * 4);
            Uint32* dstPx = (Uint32*)((Uint8*)dst->pixels + (src->h - 1 - y) * dst->pitch +
                                      (src->w - 1 - x) * 4);
            *dstPx        = *srcPx;
        }
    }
    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dst);
    return dst;
}
