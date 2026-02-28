#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <SurfaceUtils.hpp>
#include <entt/entt.hpp>
#include <vector>
#define DEBUG_HITBOXES

inline void RenderSystem(entt::registry& reg, SDL_Surface* screen) {
    // Cache format details once per frame — same for all entities
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);

    auto view = reg.view<Transform, Renderable, AnimationState>();
    view.each([&reg, screen, fmt](entt::entity          entity,
                                  const Transform&      t,
                                  Renderable&           r,
                                  const AnimationState& anim) {
        if (r.frames.empty())
            return;

        const SDL_Rect& src = r.frames[anim.currentFrame];
        auto*           g   = reg.try_get<GravityState>(entity);
        auto*           inv = reg.try_get<InvincibilityTimer>(entity);
        auto*           col = reg.try_get<Collider>(entity);

        // Fast path: no flip, no rotation — blit directly from sheet to screen
        bool needsFlip     = r.flipH;
        bool needsRotation = g && g->active && g->direction != GravityDir::DOWN;
        if (!needsFlip && !needsRotation) {
            if (inv && inv->isInvincible)
                if (static_cast<int>(inv->remaining * 10.0f) % 2 == 0)
                    SDL_SetSurfaceColorMod(r.sheet, 255, 0, 0);

            auto* roff2 = reg.try_get<RenderOffset>(entity);
            int   rx    = static_cast<int>(t.x) + (roff2 ? roff2->x : 0);
            int   ry    = static_cast<int>(t.y) + (roff2 ? roff2->y : 0);
            SDL_Rect dest = {rx, ry, src.w, src.h};
            SDL_BlitSurface(r.sheet, &src, screen, &dest);
            SDL_SetSurfaceColorMod(r.sheet, 255, 255, 255);

#ifdef DEBUG_HITBOXES
            if (col) {
                Uint32        color  = reg.all_of<PlayerTag>(entity)
                                           ? SDL_MapRGB(fmt, nullptr, 0, 255, 0)
                                           : SDL_MapRGB(fmt, nullptr, 255, 0, 0);
                constexpr int thick = 1;
                int hx = static_cast<int>(t.x), hy = static_cast<int>(t.y);
                SDL_Rect top    = {hx,           hy,           col->w, thick};
                SDL_Rect bottom = {hx,           hy + col->h,  col->w, thick};
                SDL_Rect left_  = {hx,           hy,           thick,  col->h};
                SDL_Rect right_ = {hx + col->w,  hy,           thick,  col->h};
                SDL_FillSurfaceRect(screen, &top,    color);
                SDL_FillSurfaceRect(screen, &bottom, color);
                SDL_FillSurfaceRect(screen, &left_,  color);
                SDL_FillSurfaceRect(screen, &right_, color);
            }
#endif
            return;
        }

        // Slow path: needs flip and/or rotation — extract frame into temp surface
        SDL_Surface* frame    = SDL_CreateSurface(src.w, src.h, r.sheet->format);
        bool         ownFrame = true;
        SDL_SetSurfaceBlendMode(frame, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(r.sheet, &src, frame, nullptr);

        // Horizontal flip — build per-frame cache lazily, invalidate on anim change
        if (r.flipH) {
            auto* cache = reg.try_get<FlipCache>(entity);
            if (cache && static_cast<int>(cache->frames.size()) != anim.totalFrames) {
                for (auto* s : cache->frames) if (s) SDL_DestroySurface(s);
                cache->frames.assign(anim.totalFrames, nullptr);
            }
            if (!cache) {
                reg.emplace<FlipCache>(entity);
                cache = reg.try_get<FlipCache>(entity);
                cache->frames.resize(anim.totalFrames, nullptr);
            }

            int idx = anim.currentFrame;
            if (!cache->frames[idx]) {
                SDL_Surface* flipped = SDL_CreateSurface(frame->w, frame->h, frame->format);
                SDL_SetSurfaceBlendMode(flipped, SDL_BLENDMODE_BLEND);
                SDL_LockSurface(frame);
                SDL_LockSurface(flipped);
                for (int py = 0; py < frame->h; py++)
                    for (int px = 0; px < frame->w; px++) {
                        *reinterpret_cast<Uint32*>(static_cast<Uint8*>(flipped->pixels) + py * flipped->pitch + (frame->w - 1 - px) * 4) =
                        *reinterpret_cast<Uint32*>(static_cast<Uint8*>(frame->pixels)   + py * frame->pitch   + px * 4);
                    }
                SDL_UnlockSurface(frame);
                SDL_UnlockSurface(flipped);
                cache->frames[idx] = flipped;
            }
            SDL_DestroySurface(frame);
            frame    = cache->frames[idx];
            ownFrame = false;
        }

        // Gravity rotation
        if (g && g->active) {
            SDL_Surface* rotated = nullptr;
            switch (g->direction) {
                case GravityDir::DOWN:  break;
                case GravityDir::UP:    rotated = RotateSurface180(frame);   break;
                case GravityDir::RIGHT: rotated = RotateSurface90CCW(frame); break;
                case GravityDir::LEFT:  rotated = RotateSurface90CW(frame);  break;
            }
            if (rotated) {
                if (ownFrame) SDL_DestroySurface(frame);
                frame    = rotated;
                ownFrame = true;
            }
        }

        // Wall-flush position adjustment
        // src dimensions = unrotated frame size; frame dimensions = post-rotation
        int renderX = static_cast<int>(t.x);
        int renderY = static_cast<int>(t.y);
        auto* roff  = reg.try_get<RenderOffset>(entity);

        if (g && g->active) {
            // Use src.w/h (pre-rotation) for flush so sprite sits exactly on the wall
            if (g->direction == GravityDir::RIGHT) renderX -= (frame->w - src.w);
            if (g->direction == GravityDir::UP)    renderY -= (frame->h - src.h);
            // Rotate the render offset to match the gravity direction
            if (roff) {
                switch (g->direction) {
                    case GravityDir::DOWN:  renderX += roff->x; renderY += roff->y; break;
                    case GravityDir::UP:    renderX -= roff->x; renderY -= roff->y; break;
                    case GravityDir::LEFT:  renderX += roff->y; renderY -= roff->x; break;
                    case GravityDir::RIGHT: renderX -= roff->y; renderY += roff->x; break;
                }
            }
        } else {
            if (roff) { renderX += roff->x; renderY += roff->y; }
        }

        // Invincibility flash
        if (inv && inv->isInvincible)
            if (static_cast<int>(inv->remaining * 10.0f) % 2 == 0)
                SDL_SetSurfaceColorMod(frame, 255, 0, 0);

        SDL_Rect dest = {renderX, renderY, frame->w, frame->h};
        SDL_BlitSurface(frame, nullptr, screen, &dest);
        SDL_SetSurfaceColorMod(frame, 255, 255, 255);
        int frameW = frame->w;
        if (ownFrame) SDL_DestroySurface(frame);

#ifdef DEBUG_HITBOXES
        if (col) {
            Uint32        color = reg.all_of<PlayerTag>(entity)
                                      ? SDL_MapRGB(fmt, nullptr, 0, 255, 0)
                                      : SDL_MapRGB(fmt, nullptr, 255, 0, 0);
            constexpr int thick = 1;
            int  hx = static_cast<int>(t.x);
            int  hy = static_cast<int>(t.y);
            int  cw = col->w;
            int  ch = col->h;
            if (g && g->active) {
                switch (g->direction) {
                    case GravityDir::DOWN: break;
                    case GravityDir::UP:   break;
                    case GravityDir::LEFT:
                        cw = col->h; ch = col->w;
                        break;
                    case GravityDir::RIGHT:
                        hx -= (frameW - col->w);
                        cw  = col->h; ch = col->w;
                        break;
                }
            }
            SDL_Rect top    = {hx,      hy,      cw,    thick};
            SDL_Rect bottom = {hx,      hy + ch, cw,    thick};
            SDL_Rect left_  = {hx,      hy,      thick, ch};
            SDL_Rect right_ = {hx + cw, hy,      thick, ch};
            SDL_FillSurfaceRect(screen, &top,    color);
            SDL_FillSurfaceRect(screen, &bottom, color);
            SDL_FillSurfaceRect(screen, &left_,  color);
            SDL_FillSurfaceRect(screen, &right_, color);
        }
#endif
    });
}
