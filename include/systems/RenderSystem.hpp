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
                case GravityDir::RIGHT: rotated = RotateSurface90CCW(frame); break; // bottom->right edge
                case GravityDir::LEFT:  rotated = RotateSurface90CW(frame);  break; // bottom->left edge
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
            // Flush the sprite to sit exactly at the player's world-space position.
            // t.x/t.y is always the top-left of the collider in its upright orientation.
            // After rotation the frame dimensions swap, so we must shift renderX/Y so
            // the rotated sprite's "floor" edge aligns with the collider's floor edge.
            //
            // DOWN:  no adjustment — t.x,t.y is already top-left, sprite hangs down.
            // UP:    sprite is flipped 180°; frame->h == src.h, frame->w == src.w.
            //        The top of the collider (t.y) is the floor, so the sprite's
            //        bottom edge must sit at t.y + col->h: renderY = t.y - (frame->h - col->h)
            // LEFT:  sprite rotated 90CW; frame->w == src.h, frame->h == src.w.
            //        Floor is the left wall (t.x=0); sprite hangs rightward from t.x.
            //        No X shift needed; Y needs centering offset.
            // RIGHT: sprite rotated 90CCW; frame->w == src.h, frame->h == src.w.
            //        Floor is the right wall; t.x + col->h is the wall.
            //        Sprite right edge = renderX + frame->w must equal t.x + col->h.
            //        So renderX = t.x + col->h - frame->w.
            if (col) {
                // For each wall, two things must be true:
                //   1. The sprite's "floor" edge aligns with the collider's floor edge.
                //   2. The sprite is centered over the collider on the perpendicular axis.
                //
                // col->w=32 (narrow), col->h=60 (tall/deep), frame=80x80 after rotation.
                // Centering offset on perpendicular axis = (frame_perp - col_perp) / 2
                // which equals roff->x (=-24) since roff was tuned for exactly this.
                // roff->{x,y} for DOWN gravity: x=-24 centers 80px frame over 32px collider,
                // y=-10 accounts for transparent head-padding at the top of the upright frame.
                // For other walls the axes rotate:
                //   UP    : x still centers horizontally; y=-10 is now the floor-flush (feet padding)
                //   LEFT  : roff->y=-10 becomes the X floor-flush (feet padding from left edge)
                //            roff->x=-24 centers vertically
                //   RIGHT : roff->y=-10 becomes the X floor-flush from the right edge
                //            roff->x=-24 centers vertically
                int cx = roff ? roff->x : -(frame->w - col->w) / 2; // centering offset
                int fy = roff ? roff->y : 0;                          // feet padding in upright frame
                switch (g->direction) {
                    case GravityDir::DOWN:
                        // Upright — apply full roff directly
                        if (roff) { renderX += roff->x; renderY += roff->y; }
                        break;
                    case GravityDir::UP:
                        // 180° rotation: the original sprite bottom is now at the top.
                        // t.y is the floor (ceiling wall). The sprite must hang downward
                        // from t.y. The original sprite had `fy` px of foot-padding at
                        // the bottom; after 180° that padding is now at the top of the
                        // frame, so we shift up by fy to tuck it against the wall.
                        // Full placement: sprite top = t.y + fy, hangs to t.y + fy + 80.
                        renderX += cx;
                        renderY += fy;
                        break;
                    case GravityDir::LEFT:
                        // 90CW: feet at left of frame. Feet padding (fy=-10) now on X axis.
                        // Subtract fy so the feet pixel aligns with t.x (wall at x=0).
                        renderX += fy;      // fy=-10 => renderX -= 10, pulls feet to wall
                        renderY += cx;      // center vertically (-24)
                        break;
                    case GravityDir::RIGHT:
                        // 90CCW: feet at right of frame.
                        // Base flush: renderX = t.x + col->h - frame->w
                        // Feet padding (fy=-10) on X axis: subtract to pull feet right.
                        renderX = static_cast<int>(t.x) + col->h - frame->w - fy;
                        renderY += cx;      // center vertically (-24)
                        break;
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
            // Hitbox origin = t.x, t.y — these are the true collider corners,
            // no RenderOffset involved. Just swap w/h for sidewall gravity.
            int hx = static_cast<int>(t.x);
            int hy = static_cast<int>(t.y);
            int cw = col->w;
            int ch = col->h;
            if (g && g->active &&
                (g->direction == GravityDir::LEFT || g->direction == GravityDir::RIGHT)) {
                cw = col->h;
                ch = col->w;
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
