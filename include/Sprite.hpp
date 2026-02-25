#ifndef SPRITE_HPP
#define SPRITE_HPP

#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <vector>
#include "Image.hpp"

enum class SpriteState { IDLE, WALKING, JUMPING, ATTACKING, DEAD };

class Sprite {
  public:
    Sprite(const std::vector<std::string>& framePaths,
           const SDL_PixelFormatDetails*   format,
           int frameWidth, int frameHeight,
           int x = 0, int y = 0);

    Sprite(SDL_Surface*                 spriteSheet,
           const std::vector<SDL_Rect>& frameRects,
           const SDL_PixelFormatDetails* format,
           float x = 0, float y = 0);

    Sprite(Sprite&& other) noexcept            = default;
    Sprite& operator=(Sprite&& other) noexcept = default;
    ~Sprite()                                  = default;

    void Update(float deltaTime);
    void Render(SDL_Surface* surface);

    void SetPosition(float x, float y);
    void SetScale(float scaleX, float scaleY);
    void SetRotation(double angle);
    void SetAnimationSpeed(float speed);
    void SetLooping(bool loop);
    void SetMoveSpeed(float speed);
    void SetState(SpriteState newState);

    void Play();
    void Pause();
    void Stop();
    void Reset();

    SpriteState GetState() const  { return currentState; }
    SDL_Rect    GetRect() const;
    float       GetX() const      { return positionX; }
    float       GetY() const      { return positionY; }
    int         GetWidth() const  { return static_cast<int>(frameWidth * scaleX); }
    int         GetHeight() const { return static_cast<int>(frameHeight * scaleY); }
    bool        IsPlaying() const { return isPlaying; }
    bool        IsLooping() const { return isLooping; }

    void HandleEvent(SDL_Event& event);
    bool IsPointInside(int x, int y) const;

  private:
    std::vector<std::unique_ptr<Image>> frames;
    int   currentFrame;
    int   totalFrames;

    bool  movingUp    = false;
    bool  movingDown  = false;
    bool  movingLeft  = false;
    bool  movingRight = false;
    float moveSpeed   = 150.0f;

    float  positionX;
    float  positionY;
    int    frameWidth;
    int    frameHeight;
    float  scaleX;
    float  scaleY;
    double rotation;

    void SetFlipHorizontal(bool flip);
    bool flipHorizontal = false;

    float animationSpeed;
    float frameTimer;
    bool  isPlaying;
    bool  isLooping;

    SpriteState currentState;

    void LoadFrames(const std::vector<std::string>& framePaths,
                    const SDL_PixelFormatDetails* format);
    void LoadFramesFromSheet(SDL_Surface* spriteSheet,
                             const std::vector<SDL_Rect>& frameRects,
                             const SDL_PixelFormatDetails* format);
    void AdvanceFrame();
};

#endif // SPRITE_HPP
