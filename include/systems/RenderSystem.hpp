#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <SurfaceUtils.hpp>
#include <entt/entt.hpp>
#include <vector>
#define DEBUG_HITBOXES

inline void RenderSystem(entt::registry& reg, SDL_Surface* screen) {
    auto view = reg.view<Transform, Renderable, AnimationState>();
    view.each([&reg, screen](entt::entity          entity,
                             const Transform&      t,
                             Renderable&           r,
                             const AnimationState& anim) {
        if (r.frames.empty())
            return;

        const SDL_Rect& src = r.frames[anim.currentFrame];

        // Extract current frame from original sheet — rects are always in original space
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
            ownFrame = false; // borrowed from cache — don't free
        }

        // Gravity rotation
        auto* g = reg.try_get<GravityState>(entity);
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

        // Wall-flush position adjustment after rotation
        int renderX = static_cast<int>(t.x);
        int renderY = static_cast<int>(t.y);
        if (g && g->active) {
            // After rotation, frame->w and frame->h reflect the rotated dimensions.
            // Use col to get the unrotated logical size so flush offset is always correct.
            auto* col = reg.try_get<Collider>(entity);
            int   lw  = col ? col->w : PLAYER_SPRITE_WIDTH;  // logical (unrotated) width
            int   lh  = col ? col->h : PLAYER_SPRITE_HEIGHT; // logical (unrotated) height
            if (g->direction == GravityDir::RIGHT) renderX -= (frame->w - lw);
            if (g->direction == GravityDir::UP)    renderY -= (frame->h - lh);
        }

        // Invincibility flash
        auto* inv = reg.try_get<InvincibilityTimer>(entity);
        if (inv && inv->isInvincible)
            if (static_cast<int>(inv->remaining * 10.0f) % 2 == 0)
                SDL_SetSurfaceColorMod(frame, 255, 0, 0);

        SDL_Rect dest = {renderX, renderY, frame->w, frame->h};
        SDL_BlitSurface(frame, nullptr, screen, &dest);
        SDL_SetSurfaceColorMod(frame, 255, 255, 255);
        int frameW = frame->w; // save before potential free
        int frameH = frame->h;
        if (ownFrame) SDL_DestroySurface(frame);

#ifdef DEBUG_HITBOXES
        auto* col = reg.try_get<Collider>(entity);
        if (col) {
            const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);
            Uint32 color = reg.all_of<PlayerTag>(entity)
                               ? SDL_MapRGB(fmt, nullptr, 0, 255, 0)
                               : SDL_MapRGB(fmt, nullptr, 255, 0, 0);
            constexpr int thickness = 1;
            int  hx = static_cast<int>(t.x);
            int  hy = static_cast<int>(t.y);
            int  cw = col->w;
            int  ch = col->h;
            // In gravity mode, sprite is rotated so collider axes and render offset change
            if (g && g->active) {
                switch (g->direction) {
                    case GravityDir::DOWN:
                        break;
                    case GravityDir::UP:
                        // Rotated 180 — same dimensions, no offset needed
                        break;
                    case GravityDir::LEFT:
                        // Rotated 90 CW: frame is taller than wide
                        // collider in world space: w=col->h, h=col->w
                        cw = col->h;
                        ch = col->w;
                        break;
                    case GravityDir::RIGHT:
                        hx -= (frameW - col->w);
                        cw  = col->h;
                        ch  = col->w;
                        break;
                }
            }
            SDL_Rect top    = {hx,      hy,      cw,        thickness};
            SDL_Rect bottom = {hx,      hy + ch, cw,        thickness};
            SDL_Rect left_  = {hx,      hy,      thickness, ch};
            SDL_Rect right_ = {hx + cw, hy,      thickness, ch};
            SDL_FillSurfaceRect(screen, &top,    color);
            SDL_FillSurfaceRect(screen, &bottom, color);
            SDL_FillSurfaceRect(screen, &left_,  color);
            SDL_FillSurfaceRect(screen, &right_, color);
        }
#endif
    });
}
