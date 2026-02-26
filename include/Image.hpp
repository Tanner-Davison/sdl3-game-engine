#pragma once
#include <SDL3/SDL.h>
#include <string>

enum class FitMode {
    CONTAIN,
    COVER,
    STRETCH,
    SRCSIZE,
    PRESCALED // bakes a scaled surface on first render (or on resize), then blits 1:1 every frame
};

class Image {
  public:
    Image(std::string File,
          const SDL_PixelFormatDetails* PreferredFormat = nullptr,
          FitMode mode = FitMode::CONTAIN);

    Image(std::string File);
    Image(SDL_Surface* surface, FitMode mode);

    ~Image();

    void Render(SDL_Surface* DestinationSurface);
    void SetDestinationRectangle(SDL_Rect Requested);

    Image(const Image& Source);
    Image& operator=(const Image& Source);

    void    SetFitMode(FitMode mode);
    FitMode GetFitMode() const;
    void    SetFlipHorizontal(bool flip);
    void    SaveToFile(std::string Location);

  protected:
    void HandleContain(SDL_Rect& Requested);
    void HandleCover(SDL_Rect& Requested);
    void HandleStretch(SDL_Rect& Requested);
    void HandleSrcSize(SDL_Rect& Requested);

  private:
    void RebakeScaled(int w, int h, SDL_PixelFormat destFormat);

    bool         flipHorizontal{false};
    int          destHeight{0};
    int          destWidth{0};
    int          originalWidth{0};
    int          originalHeight{0};
    SDL_Surface* mImageSurface{nullptr};
    SDL_Surface* mScaledSurface{nullptr}; // PRESCALED baked cache
    SDL_Rect     mDestRectangle{0, 0, 0, 0};
    SDL_Rect     mSrcRectangle{0, 0, 0, 0};
    FitMode      fitMode{FitMode::COVER};
    bool         destinationInitialized{false};
};
