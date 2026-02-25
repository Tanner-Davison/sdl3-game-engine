#include "Image.hpp"
#include <SDL3_image/SDL_image.h>
#include <iostream>

Image::Image(std::string File, const SDL_PixelFormatDetails* PreferredFormat,
             FitMode mode)
    : mImageSurface{IMG_Load(File.c_str())}
    , fitMode{mode} {
    if (!mImageSurface) {
        std::cout << "Failed to load image: " << File << ":\n" << SDL_GetError();
    } else {
        SDL_SetSurfaceBlendMode(mImageSurface, SDL_BLENDMODE_BLEND);
        originalWidth   = mImageSurface->w;
        originalHeight  = mImageSurface->h;
        mSrcRectangle.w = originalWidth;
        mSrcRectangle.h = originalHeight;
        SetDestinationRectangle({0, 0, 600, 300});
    }

    if (PreferredFormat) {
        SDL_PixelFormat targetFormat =
            (PreferredFormat->Amask != 0)
                ? PreferredFormat->format
                : SDL_PIXELFORMAT_RGBA8888;

        SDL_Surface* Converted = SDL_ConvertSurface(mImageSurface, targetFormat);
        if (Converted) {
            SDL_SetSurfaceBlendMode(Converted, SDL_BLENDMODE_BLEND);
            SDL_DestroySurface(mImageSurface);
            mImageSurface = Converted;
        } else {
            std::cout << "Error converting surface: " << SDL_GetError();
        }
    }
}

Image::Image(std::string File)
    : Image(File, nullptr, FitMode::CONTAIN) {}

Image::Image(SDL_Surface* surface, FitMode mode)
    : mImageSurface{surface}
    , fitMode{mode} {
    if (mImageSurface) {
        SDL_SetSurfaceBlendMode(mImageSurface, SDL_BLENDMODE_BLEND);
        originalWidth   = mImageSurface->w;
        originalHeight  = mImageSurface->h;
        mSrcRectangle.w = originalWidth;
        mSrcRectangle.h = originalHeight;
    }
}

Image::Image(const Image& Source)
    : destHeight{Source.destHeight}
    , destWidth{Source.destWidth}
    , originalWidth{Source.originalWidth}
    , originalHeight{Source.originalHeight}
    , mImageSurface{nullptr}
    , mDestRectangle{Source.mDestRectangle}
    , mSrcRectangle{Source.mSrcRectangle}
    , fitMode{Source.fitMode}
    , destinationInitialized{Source.destinationInitialized} {
    if (Source.mImageSurface) {
        mImageSurface =
            SDL_ConvertSurface(Source.mImageSurface, Source.mImageSurface->format);
        if (mImageSurface) {
            SDL_SetSurfaceBlendMode(mImageSurface, SDL_BLENDMODE_BLEND);
        }
    }
}

Image& Image::operator=(const Image& Source) {
    if (this == &Source)
        return *this;

    SDL_DestroySurface(mImageSurface);

    if (Source.mImageSurface) {
        mImageSurface =
            SDL_ConvertSurface(Source.mImageSurface, Source.mImageSurface->format);
        if (mImageSurface) {
            SDL_SetSurfaceBlendMode(mImageSurface, SDL_BLENDMODE_BLEND);
        }
    } else {
        mImageSurface = nullptr;
    }

    destHeight             = Source.destHeight;
    destWidth              = Source.destWidth;
    originalWidth          = Source.originalWidth;
    originalHeight         = Source.originalHeight;
    mDestRectangle         = Source.mDestRectangle;
    mSrcRectangle          = Source.mSrcRectangle;
    fitMode                = Source.fitMode;
    destinationInitialized = Source.destinationInitialized;

    return *this;
}

Image::~Image() {
    if (mImageSurface) {
        SDL_DestroySurface(mImageSurface);
    }
}

void Image::Render(SDL_Surface* DestinationSurface) {
    if ((fitMode == FitMode::COVER || fitMode == FitMode::CONTAIN) &&
        (destWidth != DestinationSurface->w ||
         destHeight != DestinationSurface->h || !destinationInitialized)) {
        destWidth        = DestinationSurface->w;
        destHeight       = DestinationSurface->h;
        mDestRectangle.w = destWidth;
        mDestRectangle.h = destHeight;
        SetDestinationRectangle(mDestRectangle);
        destinationInitialized = true;
    }

    if (flipHorizontal) {
        // SDL3: SDL_CreateSurface replaces SDL_CreateRGBSurfaceWithFormat
        SDL_Surface* flipped = SDL_CreateSurface(mSrcRectangle.w,
                                                  mSrcRectangle.h,
                                                  mImageSurface->format);
        SDL_SetSurfaceBlendMode(flipped, SDL_BLENDMODE_BLEND);

        SDL_LockSurface(mImageSurface);
        SDL_LockSurface(flipped);

        for (int y = 0; y < mSrcRectangle.h; y++) {
            for (int x = 0; x < mSrcRectangle.w; x++) {
                Uint32* srcPixel =
                    (Uint32*)((Uint8*)mImageSurface->pixels +
                              (mSrcRectangle.y + y) * mImageSurface->pitch +
                              (mSrcRectangle.x + x) * 4);
                Uint32* dstPixel =
                    (Uint32*)((Uint8*)flipped->pixels +
                              y * flipped->pitch +
                              (mSrcRectangle.w - 1 - x) * 4);
                *dstPixel = *srcPixel;
            }
        }

        SDL_UnlockSurface(flipped);
        SDL_UnlockSurface(mImageSurface);

        SDL_Rect srcRect = {0, 0, mSrcRectangle.w, mSrcRectangle.h};
        if (fitMode == FitMode::SRCSIZE) {
            SDL_BlitSurface(flipped, &srcRect, DestinationSurface, &mDestRectangle);
        } else {
            SDL_BlitSurfaceScaled(flipped, &srcRect, DestinationSurface,
                                  &mDestRectangle, SDL_SCALEMODE_LINEAR);
        }

        SDL_DestroySurface(flipped);
    } else {
        if (fitMode == FitMode::SRCSIZE) {
            SDL_BlitSurface(mImageSurface, &mSrcRectangle,
                            DestinationSurface, &mDestRectangle);
        } else {
            SDL_BlitSurfaceScaled(mImageSurface, &mSrcRectangle,
                                  DestinationSurface, &mDestRectangle,
                                  SDL_SCALEMODE_LINEAR);
        }
    }
}

void Image::SetFitMode(FitMode mode) { fitMode = mode; }
FitMode Image::GetFitMode() const { return fitMode; }

void Image::SetDestinationRectangle(SDL_Rect Requested) {
    switch (fitMode) {
        case FitMode::CONTAIN: HandleContain(Requested); break;
        case FitMode::COVER:   HandleCover(Requested);   break;
        case FitMode::STRETCH: HandleStretch(Requested); break;
        case FitMode::SRCSIZE: HandleSrcSize(Requested); break;
    }
}

void Image::HandleContain(SDL_Rect& Requested) {
    float SourceRatio    = originalWidth / static_cast<float>(originalHeight);
    float RequestedRatio = Requested.w / static_cast<float>(Requested.h);

    mSrcRectangle = {0, 0, originalWidth, originalHeight};
    mDestRectangle = Requested;

    if (RequestedRatio < SourceRatio) {
        mDestRectangle.h = static_cast<int>(Requested.w / SourceRatio);
    } else {
        mDestRectangle.w = static_cast<int>(Requested.h * SourceRatio);
    }

    mDestRectangle.x = (Requested.w - mDestRectangle.w) / 2;
    mDestRectangle.y = (Requested.h - mDestRectangle.h) / 2;
}

void Image::HandleCover(SDL_Rect& Requested) {
    if (originalWidth <= 0 || originalHeight <= 0) return;

    float sourceRatio    = originalWidth / static_cast<float>(originalHeight);
    float requestedRatio = Requested.w / static_cast<float>(Requested.h);

    mDestRectangle = Requested;
    mSrcRectangle  = {0, 0, originalWidth, originalHeight};

    if (requestedRatio < sourceRatio) {
        int newSrcWidth     = static_cast<int>(originalHeight * requestedRatio);
        mSrcRectangle.x     = (originalWidth - newSrcWidth) / 2;
        mSrcRectangle.w     = newSrcWidth;
    } else {
        int newSrcHeight    = static_cast<int>(originalWidth / requestedRatio);
        mSrcRectangle.y     = (originalHeight - newSrcHeight) / 2;
        mSrcRectangle.h     = newSrcHeight;
    }
}

void Image::HandleStretch(SDL_Rect& Requested) {
    mSrcRectangle  = {0, 0, originalWidth, originalHeight};
    mDestRectangle = Requested;
}

void Image::HandleSrcSize(SDL_Rect& Requested) {
    mSrcRectangle    = {0, 0, originalWidth, originalHeight};
    mDestRectangle.x = Requested.x;
    mDestRectangle.y = Requested.y;
    mDestRectangle.w = originalWidth;
    mDestRectangle.h = originalHeight;
}

void Image::SetFlipHorizontal(bool flip) { flipHorizontal = flip; }

void Image::SaveToFile(std::string Location) {
    IMG_SavePNG(mImageSurface, Location.c_str());
}
