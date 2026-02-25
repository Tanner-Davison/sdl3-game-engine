#pragma once
#include "Text.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

class ScaledText : public Text {
  public:
    ScaledText(std::string Content, int posX, int posY, int TargetWidth)
        : Text{Content, posX, posY, BaseFontSize} {
        int Width;
        TTF_GetStringSize(mFont, Content.c_str(), 0, &Width, nullptr);

        float Ratio{static_cast<float>(TargetWidth / Width)};
        int   newFontSize = static_cast<int>(BaseFontSize * Ratio);
        if (mFont) {
            TTF_CloseFont(mFont);
        }
        mFont = TTF_OpenFont("fonts/Roboto-VariableFont_wdth,wght.ttf",
                             newFontSize);
        if (!mFont) {
            std::cerr << "Error reloading the font at size " << newFontSize
                      << ": " << SDL_GetError() << "\n";
            return;
        }
        mFontSize = newFontSize;

        SetFontSize(BaseFontSize * Ratio);
        CreateSurface(Content);
    }

  private:
    static constexpr int BaseFontSize{24};
};
