#pragma once
// Image.hpp
//
// GPU-backed image with multiple viewport fit modes.
//
// ── Fit mode reference ──────────────────────────────────────────────────────
//
//   CONTAIN      Scale to fit inside viewport, preserving aspect ratio.
//                May produce letterbox/pillarbox bars. Centred.
//
//   COVER        Scale to fill viewport, preserving aspect ratio.
//                Excess is cropped. Centred.
//
//   STRETCH      Ignore aspect ratio, fill viewport exactly.
//
//   SRCSIZE      Render at native pixel dimensions at explicit position.
//
//   PRESCALED    Same as STRETCH (legacy alias for "tile" in JSON).
//
//   FILL         Aspect-preserving fill that never over-zooms large images.
//                Small images: scale up to cover. Large images (both axes
//                bigger than viewport): render at 1:1, centred.
//
//   SCROLL       Parallax scrolling background for side-scrollers.
//                Image is scaled so its HEIGHT fills the viewport (width
//                scales proportionally). The image scrolls horizontally
//                with the camera. If the scaled image is narrower than
//                the viewport, it is centred with no scroll.
//
//   SCROLL_WIDE  Source-rect scrolling for extra-wide panoramic images.
//                A viewport-shaped window slides across the texture.
//                The visible portion always fills the screen perfectly.
//                For images close to viewport aspect ratio, scroll range
//                is minimal (which is correct). For very wide panoramas,
//                there is extensive horizontal scroll.
//
// ────────────────────────────────────────────────────────────────────────────

#include <SDL3/SDL.h>
#include <string>
#include <string_view>

enum class FitMode {
    CONTAIN,
    COVER,
    STRETCH,
    SRCSIZE,
    PRESCALED,
    SCROLL,
    SCROLL_WIDE,
    FILL,
};

// Convert a level JSON string to the matching FitMode.
inline FitMode FitModeFromString(const std::string& s) {
    if (s == "contain")     return FitMode::CONTAIN;
    if (s == "cover")       return FitMode::COVER;
    if (s == "stretch")     return FitMode::STRETCH;
    if (s == "tile")        return FitMode::PRESCALED;
    if (s == "scroll")      return FitMode::SCROLL;
    if (s == "scroll_wide") return FitMode::SCROLL_WIDE;
    if (s == "fill")        return FitMode::FILL;
    return FitMode::FILL; // default
}

inline const char* FitModeToString(FitMode m) {
    switch (m) {
        case FitMode::CONTAIN:     return "contain";
        case FitMode::COVER:       return "cover";
        case FitMode::STRETCH:     return "stretch";
        case FitMode::PRESCALED:   return "tile";
        case FitMode::SCROLL:      return "scroll";
        case FitMode::SCROLL_WIDE: return "scroll_wide";
        case FitMode::FILL:        return "fill";
        case FitMode::SRCSIZE:     return "srcsize";
    }
    return "fill";
}

class Image {
  public:
    // Load from file
    Image(std::string File, FitMode mode = FitMode::CONTAIN);

    // Take ownership of an existing surface
    Image(SDL_Surface* surface, FitMode mode);

    ~Image();

    // Non-copyable (owns GPU texture)
    Image(const Image&)            = delete;
    Image& operator=(const Image&) = delete;

    // Movable
    Image(Image&&) noexcept;
    Image& operator=(Image&&) noexcept;

    // ── Rendering ────────────────────────────────────────────────────────────

    // Render for all static (non-scrolling) fit modes.
    void Render(SDL_Renderer* renderer);

    // SCROLL mode: image height fills viewport, scrolls horizontally with camera.
    //   cameraX : current camera world-space X offset (pixels).
    //   levelW  : total world width in pixels. 0 = use image's own display width
    //             as the scrollable extent (editor free-pan mode).
    void RenderScrolling(SDL_Renderer* renderer, float cameraX, float levelW = 0.0f);

    // SCROLL_WIDE mode: source-rect sliding for wide panoramas.
    //   cameraX : current camera world-space X offset (pixels).
    //   levelW  : total world width in pixels. 0 = derive from image dimensions.
    void RenderScrollingWide(SDL_Renderer* renderer, float cameraX, float levelW = 0.0f);

    // ── Configuration ────────────────────────────────────────────────────────

    void SetDestinationRectangle(SDL_FRect dest);

    void    SetFitMode(FitMode mode);
    FitMode GetFitMode() const;
    void    SetFlipHorizontal(bool flip);
    void    SetRepeat(bool repeat);
    bool    GetRepeat() const;

    void SaveToFile(std::string Location);

    int GetOriginalWidth()  const { return mOrigW; }
    int GetOriginalHeight() const { return mOrigH; }

  private:
    void      UploadSurface(SDL_Renderer* renderer);
    SDL_FRect ComputeDest(int rendW, int rendH) const;

    SDL_Surface* mPendingSurface = nullptr;
    SDL_Texture* mTexture        = nullptr;
    int          mOrigW          = 0;
    int          mOrigH          = 0;
    FitMode      mFitMode        = FitMode::CONTAIN;
    bool         mFlipH          = false;

    SDL_FRect mExplicitDest{0, 0, 0, 0};
    bool      mHasExplicitDest = false;
    bool      mRepeat          = false;
};
