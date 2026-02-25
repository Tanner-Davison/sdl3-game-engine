#include "Sprite.hpp"
#include <iostream>

Sprite::Sprite(const std::vector<std::string>& framePaths,
               const SDL_PixelFormatDetails*   format,
               int frameWidth, int frameHeight, int x, int y)
    : currentFrame(0), totalFrames(0)
    , positionX(x), positionY(y)
    , frameWidth(frameWidth), frameHeight(frameHeight)
    , scaleX(1.0f), scaleY(1.0f), rotation(0.0)
    , animationSpeed(1.0f), frameTimer(0.0f)
    , isPlaying(false), isLooping(true)
    , currentState(SpriteState::IDLE) {
    LoadFrames(framePaths, format);
}

Sprite::Sprite(SDL_Surface* spriteSheet,
               const std::vector<SDL_Rect>& frameRects,
               const SDL_PixelFormatDetails* format,
               float x, float y)
    : currentFrame(0), totalFrames(0)
    , positionX(x), positionY(y)
    , frameWidth(0), frameHeight(0)
    , scaleX(1.0f), scaleY(1.0f), rotation(0.0)
    , animationSpeed(1.0f), frameTimer(0.0f)
    , isPlaying(true), isLooping(true)
    , currentState(SpriteState::IDLE) {
    LoadFramesFromSheet(spriteSheet, frameRects, format);
}

void Sprite::LoadFramesFromSheet(SDL_Surface* spriteSheet,
                                 const std::vector<SDL_Rect>& frameRects,
                                 const SDL_PixelFormatDetails* format) {
    frames.clear();

    if (frameRects.empty()) return;

    frameWidth  = frameRects[0].w;
    frameHeight = frameRects[0].h;

    for (const auto& srcRect : frameRects) {
        // SDL3: SDL_CreateSurface replaces SDL_CreateRGBSurfaceWithFormat
        SDL_Surface* frameSurface = SDL_CreateSurface(srcRect.w, srcRect.h,
                                                       SDL_PIXELFORMAT_RGBA8888);
        if (!frameSurface) continue;

        SDL_SetSurfaceBlendMode(frameSurface, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(spriteSheet, &srcRect, frameSurface, nullptr);

        frames.push_back(std::make_unique<Image>(frameSurface, FitMode::SRCSIZE));
    }

    totalFrames  = static_cast<int>(frames.size());
    currentFrame = 0;
}

void Sprite::LoadFrames(const std::vector<std::string>& framePaths,
                        const SDL_PixelFormatDetails* format) {
    frames.clear();
    for (const auto& path : framePaths) {
        frames.push_back(std::make_unique<Image>(path, format, FitMode::SRCSIZE));
    }
    totalFrames  = static_cast<int>(frames.size());
    currentFrame = 0;
}

void Sprite::Update(float deltaTime) {
    bool isMoving = movingUp || movingDown || movingLeft || movingRight;

    if (movingLeft) {
        SetFlipHorizontal(true);
    } else if (movingRight) {
        SetFlipHorizontal(false);
    }

    if (isMoving && totalFrames > 1) {
        const float maxDeltaTime = 0.1f;
        deltaTime = std::min(deltaTime, maxDeltaTime);

        float frameInterval = 1.0f / animationSpeed;
        frameTimer += deltaTime;

        while (frameTimer >= frameInterval) {
            AdvanceFrame();
            frameTimer -= frameInterval;
        }
    } else {
        currentFrame = 0;
        frameTimer   = 0.0f;
    }

    float moveDistance = moveSpeed * deltaTime;
    if (movingUp)    positionY -= moveDistance;
    if (movingDown)  positionY += moveDistance;
    if (movingLeft)  positionX -= moveDistance;
    if (movingRight) positionX += moveDistance;
}

void Sprite::SetMoveSpeed(float speed) { moveSpeed = speed; }

void Sprite::SetFlipHorizontal(bool flip) {
    flipHorizontal = flip;
    for (auto& frame : frames) {
        frame->SetFlipHorizontal(flip);
    }
}

void Sprite::AdvanceFrame() {
    currentFrame++;
    if (currentFrame >= totalFrames) {
        if (isLooping) {
            currentFrame = 0;
        } else {
            currentFrame = totalFrames - 1;
            isPlaying    = false;
        }
    }
}

void Sprite::Render(SDL_Surface* surface) {
    if (frames.empty() || currentFrame >= totalFrames) return;

    SDL_Rect destRect = {
        static_cast<int>(positionX),
        static_cast<int>(positionY),
        static_cast<int>(frameWidth * scaleX),
        static_cast<int>(frameHeight * scaleY)};

    frames[currentFrame]->SetDestinationRectangle(destRect);
    frames[currentFrame]->Render(surface);
}

void Sprite::SetPosition(float x, float y) { positionX = x; positionY = y; }
void Sprite::SetScale(float sx, float sy)  { scaleX = sx; scaleY = sy; }
void Sprite::SetRotation(double angle)     { rotation = angle; }
void Sprite::SetAnimationSpeed(float speed){ animationSpeed = speed; }
void Sprite::SetLooping(bool loop)         { isLooping = loop; }
void Sprite::Play()  { isPlaying = true; }
void Sprite::Pause() { isPlaying = false; }
void Sprite::Stop()  { isPlaying = false; Reset(); }
void Sprite::Reset() { currentFrame = 0; frameTimer = 0.0f; }
void Sprite::SetState(SpriteState newState) { currentState = newState; }

SDL_Rect Sprite::GetRect() const {
    return {static_cast<int>(positionX), static_cast<int>(positionY),
            static_cast<int>(frameWidth * scaleX),
            static_cast<int>(frameHeight * scaleY)};
}

void Sprite::HandleEvent(SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (IsPointInside(static_cast<int>(event.button.x),
                              static_cast<int>(event.button.y))) {
                isPlaying ? Pause() : Play();
            }
        }
    }

    // SDL3: event.key.key instead of event.key.keysym.sym
    if (event.type == SDL_EVENT_KEY_DOWN) {
        switch (event.key.key) {
            case SDLK_W: movingUp    = true; break;
            case SDLK_A: movingLeft  = true; break;
            case SDLK_S: movingDown  = true; break;
            case SDLK_D: movingRight = true; break;
        }
    }

    if (event.type == SDL_EVENT_KEY_UP) {
        switch (event.key.key) {
            case SDLK_W: movingUp    = false; break;
            case SDLK_A: movingLeft  = false; break;
            case SDLK_S: movingDown  = false; break;
            case SDLK_D: movingRight = false; break;
        }
    }
}

bool Sprite::IsPointInside(int x, int y) const {
    SDL_Rect rect = GetRect();
    return (x >= rect.x && x <= rect.x + rect.w &&
            y >= rect.y && y <= rect.y + rect.h);
}
