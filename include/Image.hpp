#pragma once
#include <SDL.h>
#include <SDL_surface.h>
#include <string>

/**
 * @brief Controls how an image is scaled to fit its destination rectangle.
 */
enum class FitMode {
    CONTAIN, ///< Scale to fit within the destination while preserving aspect ratio (letterboxed).
    COVER,   ///< Scale to fill the destination while preserving aspect ratio (cropped).
    STRETCH, ///< Stretch to exactly fill the destination, ignoring aspect ratio.
    SRCSIZE  ///< Use the image's original pixel dimensions without scaling.
};

/**
 * @class Image
 * @brief Manages loading, scaling, and blitting a single SDL surface image.
 *
 * Handles image loading from disk (BMP, PNG, JPG via SDL_image), optional
 * pixel format conversion for fast blitting, and FitMode-based scaling logic.
 * Supports horizontal flipping and saving to PNG.
 *
 * @see FitMode
 */
class Image {
  public:
    /**
     * @brief Loads an image from disk with optional format conversion and fit mode.
     *
     * @param File            Path to the image file (supports BMP, PNG, JPG, etc.)
     * @param PreferredFormat Optional pixel format to convert the surface to for
     *                        optimized blitting. Pass nullptr to skip conversion.
     * @param mode            How the image should scale to fit its destination.
     */
    Image(std::string      File,
          SDL_PixelFormat* PreferredFormat = nullptr,
          FitMode          mode            = FitMode::CONTAIN);

    /**
     * @brief Loads an image from disk using default settings (no format conversion, CONTAIN fit).
     * @param File Path to the image file.
     */
    Image(std::string File);

    /**
     * @brief Wraps an existing SDL_Surface with a specified fit mode.
     * @param surface An already-loaded SDL_Surface (not owned — caller manages lifetime).
     * @param mode    How the image should scale to fit its destination.
     */
    Image(SDL_Surface* surface, FitMode mode);

    /// Frees the managed SDL_Surface if owned.
    ~Image();

    /**
     * @brief Blits the image onto a destination surface using the current fit and destination rect.
     * @param DestinationSurface The surface to draw onto (typically the window surface).
     */
    void Render(SDL_Surface* DestinationSurface);

    /**
     * @brief Sets the destination rectangle for rendering.
     *
     * The image will be scaled/positioned within this rect according to the current FitMode.
     * @param Requested The desired destination rect on the target surface.
     */
    void SetDestinationRectangle(SDL_Rect Requested);

    /// Copy constructor — performs a deep copy of the underlying SDL_Surface.
    Image(const Image& Source);

    /// Copy assignment — performs a deep copy of the underlying SDL_Surface.
    Image& operator=(const Image& Source);

    /**
     * @brief Changes the fit mode used when rendering.
     * @param mode The new FitMode to apply.
     */
    void    SetFitMode(FitMode mode);

    /// Returns the current FitMode.
    FitMode GetFitMode() const;

    /**
     * @brief Enables or disables horizontal flipping when rendering.
     * @param flip true to flip horizontally, false for normal orientation.
     */
    void SetFlipHorizontal(bool flip);

    /**
     * @brief Saves the current image surface to a PNG file.
     * @param Location Output file path (e.g. "screenshot.png").
     */
    void SaveToFile(std::string Location);

  protected:
    void HandleContain(SDL_Rect& Requested); ///< Scales image to fit within rect preserving ratio.
    void HandleCover(SDL_Rect& Requested);   ///< Scales image to fill rect preserving ratio.
    void HandleStretch(SDL_Rect& Requested); ///< Stretches image to exactly fill rect.
    void HandleSrcSize(SDL_Rect& Requested); ///< Uses original image dimensions.

  private:
    bool         flipHorizontal{false};
    int          destHeight{0};
    int          destWidth{0};
    int          originalWidth{0};
    int          originalHeight{0};
    SDL_Surface* mImageSurface{nullptr};
    SDL_Rect     mDestRectangle{0, 0, 0, 0};
    SDL_Rect     mSrcRectangle{0, 0, 0, 0};
    FitMode      fitMode{FitMode::COVER};
    bool         destinationInitialized{false};
};
