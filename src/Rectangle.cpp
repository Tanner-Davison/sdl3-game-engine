#include "Rectangle.hpp"
#include <iostream>
#include <SDL3/SDL.h>

Rectangle::Rectangle(const SDL_Rect& _Rect) : Rect(_Rect) {}

void Rectangle::Render(SDL_Surface* Surface) const {
    auto [r, g, b, a]{isPointerHovering ? HoverColor : Color};
    const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(Surface->format);
    SDL_FillSurfaceRect(Surface, &Rect, SDL_MapRGB(details, nullptr, r, g, b));
}

void Rectangle::HandleEvent(SDL_Event& E) {
    if (E.type == SDL_EVENT_MOUSE_MOTION) {
        bool wasPointerHovering{isPointerHovering};
        isPointerHovering = isWithinRect(static_cast<int>(E.motion.x),
                                         static_cast<int>(E.motion.y));
        if (!wasPointerHovering && isPointerHovering) {
            OnMouseEnter();
        } else if (wasPointerHovering && !isPointerHovering) {
            OnMouseExit();
        }
    } else if (E.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        if (isPointerHovering) OnMouseExit();
        isPointerHovering = false;
    } else if (E.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (isPointerHovering && E.button.button == SDL_BUTTON_LEFT) {
            OnLeftClick();
        }
    }
}

void Rectangle::OnMouseEnter() {}
void Rectangle::OnMouseExit()  {}
void Rectangle::OnLeftClick()  {}

bool Rectangle::isWithinRect(int x, int y) const {
    return (x >= Rect.x && x <= Rect.x + Rect.w &&
            y >= Rect.y && y <= Rect.y + Rect.h);
}

void Rectangle::SetColor(const SDL_Color& NewColor)      { Color = NewColor; }
void Rectangle::SetHoverColor(const SDL_Color& NewColor)  { HoverColor = NewColor; }
SDL_Color Rectangle::GetColor() const      { return Color; }
SDL_Color Rectangle::GetHoverColor() const { return HoverColor; }
