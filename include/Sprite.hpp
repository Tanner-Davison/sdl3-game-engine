#ifndef SPRITE_HPP
#define SPRITE_HPP

#include <SDL.h>
#include <memory>
#include <string>
#include <vector>

#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include "Image.hpp"

/**
 * @brief Represents the current behavioral state of a Sprite.
 *
 * Used to track what a sprite is doing, which can drive animation selection
 * or logic branching in game systems.
 */
enum class SpriteState {
    IDLE,      ///< The sprite is not moving or performing any action.
    WALKING,   ///< The sprite is walking.
    JUMPING,   ///< The sprite is in the air.
    ATTACKING, ///< The sprite is performing an attack.
    DEAD       ///< The sprite has been destroyed or killed.
};

/**
 * @class Sprite
 * @brief An animated 2D entity that renders frames from an image strip or sprite sheet.
 *
 * Sprites can be constructed from a list of individual image files or from a
 * pre-loaded sprite sheet surface with explicit frame rectangles. They support
 * delta-time based animation, keyboard-driven movement, scaling, flipping, and
 * state management.
 *
 * @par Example (from sprite sheet)
 * @code
 * SpriteSheet sheet("assets/player.png", "assets/player.txt");
 * std::vector<SDL_Rect> walkFrames = sheet.GetAnimation("p1_walk");
 * Sprite player(sheet.GetSurface(), walkFrames, window.GetSurface()->format, 100, 200);
 * player.SetAnimationSpeed(12.0f);
 * player.SetLooping(true);
 * @endcode
 *
 * @see SpriteSheet
 * @see SpriteState
 * @see Image
 */
class Sprite {
  public:
    /**
     * @brief Constructs a Sprite from individual frame image files.
     *
     * @param framePaths  List of file paths for each animation frame, in order.
     * @param format      Pixel format to convert frames to for optimized blitting.
     * @param frameWidth  Width of each frame in pixels.
     * @param frameHeight Height of each frame in pixels.
     * @param x           Initial X position on screen.
     * @param y           Initial Y position on screen.
     */
    Sprite(const std::vector<std::string>& framePaths,
           SDL_PixelFormat*                format,
           int                             frameWidth,
           int                             frameHeight,
           int                             x = 0,
           int                             y = 0);

    /**
     * @brief Constructs a Sprite from a sprite sheet surface and frame rectangles.
     *
     * @param spriteSheet  Pointer to the full sprite sheet SDL_Surface (not owned).
     * @param frameRects   Ordered list of SDL_Rect regions defining each frame.
     * @param format       Pixel format to convert frames to for optimized blitting.
     * @param x            Initial X position on screen.
     * @param y            Initial Y position on screen.
     */
    Sprite(SDL_Surface*                 spriteSheet,
           const std::vector<SDL_Rect>& frameRects,
           SDL_PixelFormat*             format,
           float                        x = 0,
           float                        y = 0);

    Sprite(Sprite&& other) noexcept            = default;
    Sprite& operator=(Sprite&& other) noexcept = default;
    ~Sprite()                                  = default;

    /**
     * @brief Advances animation and position each frame.
     *
     * Should be called once per game loop iteration. Handles frame timing,
     * movement direction flags set by HandleEvent, and state transitions.
     *
     * @param deltaTime Time elapsed since the last frame, in seconds.
     */
    void Update(float deltaTime);

    /**
     * @brief Draws the current animation frame to a surface.
     * @param surface The destination SDL_Surface to render onto.
     */
    void Render(SDL_Surface* surface);

    /**
     * @brief Sets the sprite's world position.
     * @param x X coordinate in pixels.
     * @param y Y coordinate in pixels.
     */
    void SetPosition(float x, float y);

    /**
     * @brief Scales the sprite relative to its original frame size.
     * @param scaleX Horizontal scale factor (1.0 = original size).
     * @param scaleY Vertical scale factor (1.0 = original size).
     */
    void SetScale(float scaleX, float scaleY);

    /**
     * @brief Sets the rotation angle for rendering (currently informational).
     * @param angle Rotation in degrees.
     */
    void SetRotation(double angle);

    /**
     * @brief Sets the SDL render flip mode.
     * @param flip SDL_RendererFlip value (SDL_FLIP_NONE, SDL_FLIP_HORIZONTAL, etc.).
     */
    void SetFlip(SDL_RendererFlip flip);

    /**
     * @brief Sets how many animation frames play per second.
     * @param speed Frames per second for the animation.
     */
    void SetAnimationSpeed(float speed);

    /**
     * @brief Enables or disables looping animation playback.
     * @param loop true to loop continuously, false to stop at the last frame.
     */
    void SetLooping(bool loop);

    void Play();  ///< Resumes or starts animation playback.
    void Pause(); ///< Pauses animation at the current frame.
    void Stop();  ///< Stops animation and resets to the first frame.
    void Reset(); ///< Resets the current frame index to 0.

    /**
     * @brief Transitions the sprite to a new behavioral state.
     * @param newState The SpriteState to transition to.
     */
    void SetState(SpriteState newState);

    /// Returns the sprite's current SpriteState.
    SpriteState GetState() const {
        return currentState;
    }

    /// Returns the bounding rectangle of the sprite at its current position.
    SDL_Rect GetRect() const;

    float GetX() const { return positionX; }   ///< Returns current X position.
    float GetY() const { return positionY; }   ///< Returns current Y position.
    int   GetWidth() const { return frameWidth * scaleX; }  ///< Returns scaled width in pixels.
    int   GetHeight() const { return frameHeight * scaleY; } ///< Returns scaled height in pixels.

    /**
     * @brief Sets the movement speed in pixels per second.
     * @param speed Movement speed (pixels/second).
     */
    void SetMoveSpeed(float speed);

    bool IsPlaying() const { return isPlaying; } ///< Returns true if animation is currently playing.
    bool IsLooping() const { return isLooping; } ///< Returns true if animation is set to loop.

    /**
     * @brief Processes SDL input events for keyboard-driven movement.
     *
     * Listens for WASD/arrow key presses and releases to set movement direction flags
     * that Update() uses to translate the sprite each frame.
     *
     * @param event The SDL_Event to process.
     */
    void HandleEvent(SDL_Event& event);

    /**
     * @brief Tests whether a screen coordinate falls inside the sprite's bounds.
     * @param x X coordinate to test.
     * @param y Y coordinate to test.
     * @return true if (x, y) is inside the sprite's bounding rectangle.
     */
    bool IsPointInside(int x, int y) const;

  private:
    std::vector<std::unique_ptr<Image>> frames; ///< Loaded animation frames.
    int                                 currentFrame;
    int                                 totalFrames;

    bool  movingUp    = false;
    bool  movingDown  = false;
    bool  movingLeft  = false;
    bool  movingRight = false;
    float moveSpeed   = 150.0f; ///< Movement speed in pixels per second.

    float positionX;
    float positionY;
    int   frameWidth;
    int   frameHeight;

    float  scaleX;
    float  scaleY;
    double rotation;

    void             SetFlipHorizontal(bool flip);
    SDL_RendererFlip flip;

    float animationSpeed; ///< Frames per second for animation playback.
    float frameTimer;     ///< Accumulated time since the last frame advance.
    bool  isPlaying;
    bool  isLooping;

    SpriteState currentState;

    /// Loads frames from individual image files.
    void LoadFrames(const std::vector<std::string>& framePaths,
                    SDL_PixelFormat*                format);

    /// Extracts and loads frames from a sprite sheet using the provided rects.
    void LoadFramesFromSheet(SDL_Surface*                 spriteSheet,
                             const std::vector<SDL_Rect>& frameRects,
                             SDL_PixelFormat*             format);

    /// Moves to the next frame, respecting loop and play state.
    void AdvanceFrame();
};

#endif // SPRITE_HPP
