#pragma once
#include "Components.hpp"
#include "Image.hpp"
#include "Level.hpp"
#include "LevelSerializer.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpriteSheet.hpp"
#include "Systems.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstdlib>
#include <entt/entt.hpp>
#include <memory>
#include <print>

class GameScene : public Scene {
  public:
    // Default constructor — uses hardcoded level data
    GameScene() = default;

    // Load from a saved level file
    explicit GameScene(const std::string& levelPath) : mLevelPath(levelPath) {}

    void Load(Window& window) override {
        mWindow  = &window;
        gameOver = false;

        // Load level from file if a path was provided
        if (!mLevelPath.empty())
            LoadLevel(mLevelPath, mLevel);

        // Frost Knight — individual PNG sequences, zero-padded 3-digit frame numbers
        constexpr int KW = 80, KH = 80; // render size
        auto loadAnim = [&](const std::string& folder, const std::string& prefix, int count) {
            return std::make_unique<SpriteSheet>(
                "game_assets/frost_knight_png_sequences/" + folder + "/",
                prefix, count, KW, KH, 3);
        };

        knightIdleSheet   = loadAnim("Idle",         "0_Knight_Idle_",         18);
        knightWalkSheet   = loadAnim("Walking",      "0_Knight_Walking_",      24);
        knightHurtSheet   = loadAnim("Hurt",         "0_Knight_Hurt_",         12);
        knightJumpSheet   = loadAnim("Jump Start",   "0_Knight_Jump Start_",    6);
        knightFallSheet   = loadAnim("Falling Down", "0_Knight_Falling Down_",  6);
        knightSlideSheet  = loadAnim("Sliding",      "0_Knight_Sliding_",       6);

        idleFrames  = knightIdleSheet->GetAnimation("0_Knight_Idle_");
        walkFrames  = knightWalkSheet->GetAnimation("0_Knight_Walking_");
        jumpFrames  = knightJumpSheet->GetAnimation("0_Knight_Jump Start_");
        hurtFrames  = knightHurtSheet->GetAnimation("0_Knight_Hurt_");
        duckFrames  = knightSlideSheet->GetAnimation("0_Knight_Sliding_");
        frontFrames = knightFallSheet->GetAnimation("0_Knight_Falling Down_");

        enemySheet = std::make_unique<SpriteSheet>(
            "game_assets/base_pack/Enemies/enemies_spritesheet.png",
            "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
        enemyWalkFrames = enemySheet->GetAnimation("slimeWalk");

        background = std::make_unique<Image>(
            "game_assets/base_pack/deepspace_scene.png", nullptr, FitMode::PRESCALED);
        locationText = std::make_unique<Text>("You are in space!!", 20, 20);

        actionText = std::make_unique<Text>(
            "Level 1: Collect ALL the coins!", SDL_Color{255, 255, 255, 0}, 20, 80, 20);

        gameOverText = std::make_unique<Text>("Game Over!",
                                              SDL_Color{255, 0, 0, 255},
                                              window.GetWidth() / 2 - 100,
                                              window.GetHeight() / 2 - 60,
                                              64);
        retryBtnText = std::make_unique<Text>("Retry",
                                              SDL_Color{0, 0, 0, 255},
                                              window.GetWidth() / 2 - 28,
                                              window.GetHeight() / 2 + 22,
                                              32);
        retryKeyText = std::make_unique<Text>("Press R to Retry",
                                              SDL_Color{200, 200, 200, 255},
                                              window.GetWidth() / 2 - 100,
                                              window.GetHeight() / 2 + 110,
                                              24);

        retryBtnRect = {window.GetWidth() / 2 - 75, window.GetHeight() / 2 + 10, 150, 55};
        retryButton  = std::make_unique<Rectangle>(retryBtnRect);
        retryButton->SetColor({255, 255, 255, 255});
        retryButton->SetHoverColor({180, 180, 180, 255});

        levelCompleteText = std::make_unique<Text>("Level Complete!",
                                                   SDL_Color{255, 215, 0, 255},
                                                   window.GetWidth() / 2 - 160,
                                                   window.GetHeight() / 2 - 40,
                                                   64);
        Spawn();
    }

    void Unload() override {
        reg.clear();
        for (auto* s : tileScaledSurfaces) SDL_DestroySurface(s);
        tileScaledSurfaces.clear();
        mWindow = nullptr;
    }

    std::unique_ptr<Scene> NextScene() override;

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT)
            return false;

        if (!gameOver) {
            InputSystem(reg, e);
        } else {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R) {
                Respawn();
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                e.button.button == SDL_BUTTON_LEFT) {
                int mx = (int)e.button.x;
                int my = (int)e.button.y;
                if (mx >= retryBtnRect.x && mx <= retryBtnRect.x + retryBtnRect.w &&
                    my >= retryBtnRect.y && my <= retryBtnRect.y + retryBtnRect.h) {
                    Respawn();
                }
            }
            retryButton->HandleEvent(e);
        }
        return true;
    }

    void Update(float dt) override {
        if (levelComplete) {
            levelCompleteTimer -= dt;
            return;
        }
        if (!gameOver) {
            MovementSystem(reg, dt, mWindow->GetWidth());
            CenterPullSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
            BoundsSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
            PlayerStateSystem(reg);
            AnimationSystem(reg, dt);
            CollisionSystem(reg, dt, gameOver, coinCount, stompCount, mWindow->GetWidth(), mWindow->GetHeight());

            // Check win condition
            if (totalCoins > 0 && coinCount >= totalCoins)
                levelComplete = true;
        }
    }

    void Render(Window& window) override {
        window.Render();
        background->Render(window.GetSurface());

        if (levelComplete) {
            RenderSystem(reg, window.GetSurface());
            HUDSystem(reg,
                      window.GetSurface(),
                      window.GetWidth(),
                      healthText.get(),
                      gravityText.get(),
                      coinText.get(),
                      coinCount,
                      stompText.get(),
                      stompCount);
            if (levelCompleteText)
                levelCompleteText->Render(window.GetSurface());
        } else if (gameOver) {
            gameOverText->Render(window.GetSurface());
            retryButton->Render(window.GetSurface());
            retryBtnText->Render(window.GetSurface());
            retryKeyText->Render(window.GetSurface());
        } else {
            locationText->Render(window.GetSurface());
            actionText->Render(window.GetSurface());
            RenderSystem(reg, window.GetSurface());
            HUDSystem(reg,
                      window.GetSurface(),
                      window.GetWidth(),
                      healthText.get(),
                      gravityText.get(),
                      coinText.get(),
                      coinCount,
                      stompText.get(),
                      stompCount);
        }

        window.Update();
    }

  private:
    entt::registry reg;
    bool           gameOver           = false;
    bool           levelComplete      = false;
    float          levelCompleteTimer = 2.0f;
    int            totalCoins         = 0;
    Window*        mWindow            = nullptr;
    std::string    mLevelPath         = ""; // empty = use hardcoded defaults
    Level          mLevel;                  // populated from file if mLevelPath is set

    // Knight animation sheets (one per animation since they're separate PNG sequences)
    std::unique_ptr<SpriteSheet> knightIdleSheet;
    std::unique_ptr<SpriteSheet> knightWalkSheet;
    std::unique_ptr<SpriteSheet> knightHurtSheet;
    std::unique_ptr<SpriteSheet> knightJumpSheet;
    std::unique_ptr<SpriteSheet> knightFallSheet;
    std::unique_ptr<SpriteSheet> knightSlideSheet;
    std::unique_ptr<SpriteSheet> enemySheet;
    std::unique_ptr<SpriteSheet> coinSheet;
    std::vector<SDL_Surface*>    tileScaledSurfaces; // owned tile surfaces freed on unload
    std::vector<SDL_Rect>        walkFrames;
    std::vector<SDL_Rect>        jumpFrames;
    std::vector<SDL_Rect>        idleFrames;
    std::vector<SDL_Rect>        hurtFrames;
    std::vector<SDL_Rect>        duckFrames;
    std::vector<SDL_Rect>        frontFrames;
    std::vector<SDL_Rect>        enemyWalkFrames;

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      locationText;
    std::unique_ptr<Text>      actionText;
    std::unique_ptr<Text>      gameOverText;
    std::unique_ptr<Text>      retryBtnText;
    std::unique_ptr<Text>      retryKeyText;
    std::unique_ptr<Rectangle> retryButton;
    std::unique_ptr<Text>      healthText;
    std::unique_ptr<Text>      gravityText;
    std::unique_ptr<Text>      coinText;
    std::unique_ptr<Text>      stompText;
    std::unique_ptr<Text>      levelCompleteText;
    int                        coinCount  = 0;
    int                        stompCount = 0;
    SDL_Rect                   retryBtnRect{};

    void Spawn() {
        healthText  = std::make_unique<Text>("100", SDL_Color{255, 255, 255, 255}, 0, 0, 16);
        gravityText = std::make_unique<Text>("", SDL_Color{100, 200, 255, 255}, 0, 0, 20);
        coinText   = std::make_unique<Text>("Gold Collected: 0",  SDL_Color{255, 215, 0, 255}, 0, 0, 16);
        stompText  = std::make_unique<Text>("Enemies Stomped: 0", SDL_Color{255, 100, 100, 255}, 0, 0, 16);
        coinSheet =
            std::make_unique<SpriteSheet>("game_assets/gold_coins/", "Gold_", 30, 40, 40);
        std::vector<SDL_Rect> coinFrames = coinSheet->GetAnimation("Gold_");

        // Spawn coins — from level file if loaded, otherwise random wall placement
        auto spawnCoin = [&](float cx, float cy) {
            auto coin = reg.create();
            reg.emplace<Transform>(coin, cx, cy);
            reg.emplace<Renderable>(coin, coinSheet->GetSurface(), coinFrames, false);
            reg.emplace<AnimationState>(coin, 0, (int)coinFrames.size(), 0.0f, 15.0f, true);
            reg.emplace<Collider>(coin, COIN_SIZE, COIN_SIZE);
            reg.emplace<CoinTag>(coin);
        };

        if (!mLevelPath.empty() && !mLevel.coins.empty()) {
            for (const auto& c : mLevel.coins)
                spawnCoin(c.x, c.y);
        } else {
            for (int i = 0; i < COIN_COUNT; i++) {
                int   wall = rand() % 3;
                float cx = 0.0f, cy = 0.0f;
                int   pad = COIN_SIZE + 10;
                switch (wall) {
                    case 0: cx = 5.0f;                                                              cy = static_cast<float>(pad + rand() % (mWindow->GetHeight() - pad * 2)); break;
                    case 1: cx = static_cast<float>(pad + rand() % (mWindow->GetWidth() - pad * 2)); cy = 5.0f; break;
                    case 2: cx = static_cast<float>(mWindow->GetWidth() - COIN_SIZE - 5);          cy = static_cast<float>(pad + rand() % (mWindow->GetHeight() - pad * 2)); break;
                }
                spawnCoin(cx, cy);
            }
        }

        // Count total coins so we know the win condition
        totalCoins = static_cast<int>(reg.view<CoinTag>().size());

        // Player spawn — use level data if loaded, otherwise default to bottom center
        float playerX = mLevelPath.empty()
                          ? (float)(mWindow->GetWidth() / 2 - 33)
                          : mLevel.player.x;
        float playerY = mLevelPath.empty()
                          ? (float)(mWindow->GetHeight() - PLAYER_SPRITE_HEIGHT)
                          : mLevel.player.y;

        auto player = reg.create();
        reg.emplace<Transform>(player, playerX, playerY);
        reg.emplace<Velocity>(player);
        reg.emplace<AnimationState>(player, 0, (int)idleFrames.size(), 0.0f, 10.0f, true);
        reg.emplace<Renderable>(player, knightIdleSheet->GetSurface(), idleFrames, false);
        reg.emplace<PlayerTag>(player);
        reg.emplace<Health>(player);
        reg.emplace<Collider>(player, 32, 60); // knight body occupies center of 80x80 frame
        reg.emplace<RenderOffset>(player, -24, -10); // offset so sprite aligns over hitbox
        reg.emplace<InvincibilityTimer>(player);
        reg.emplace<GravityState>(player);
        reg.emplace<AnimationSet>(player, AnimationSet{
            .idle       = idleFrames,  .idleSheet  = knightIdleSheet->GetSurface(),
            .walk       = walkFrames,  .walkSheet  = knightWalkSheet->GetSurface(),
            .jump       = jumpFrames,  .jumpSheet  = knightJumpSheet->GetSurface(),
            .hurt       = hurtFrames,  .hurtSheet  = knightHurtSheet->GetSurface(),
            .duck       = duckFrames,  .duckSheet  = knightSlideSheet->GetSurface(),
            .front      = frontFrames, .frontSheet = knightFallSheet->GetSurface(),
        });

        // Spawn tiles — only from level file
        for (const auto& ts : mLevel.tiles) {
            SDL_Surface* tileSurf = IMG_Load(ts.imagePath.c_str());
            if (!tileSurf) {
                std::print("Failed to load tile: {}\n", ts.imagePath);
                continue;
            }
            SDL_SetSurfaceBlendMode(tileSurf, SDL_BLENDMODE_BLEND);
            // Scale to requested size
            SDL_Surface* scaled = SDL_CreateSurface(ts.w, ts.h, tileSurf->format);
            SDL_Rect src = {0, 0, tileSurf->w, tileSurf->h};
            SDL_Rect dst = {0, 0, ts.w, ts.h};
            SDL_BlitSurfaceScaled(tileSurf, &src, scaled, &dst, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(tileSurf);
            SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);

            auto tile = reg.create();
            reg.emplace<Transform>(tile, ts.x, ts.y);
            reg.emplace<Collider>(tile, ts.w, ts.h);
            reg.emplace<TileTag>(tile);
            // Single-frame renderable using scaled surface directly
            std::vector<SDL_Rect> tileFrame = {{0, 0, ts.w, ts.h}};
            reg.emplace<Renderable>(tile, scaled, tileFrame, false);
            reg.emplace<AnimationState>(tile, 0, 1, 0.0f, 1.0f, false);
            tileScaledSurfaces.push_back(scaled);
        }

        // Spawn enemies — from level file if loaded, otherwise random placement
        auto spawnEnemy = [&](float x, float y, float speed) {
            float dx = (rand() % 2 == 0) ? speed : -speed;
            auto  enemy = reg.create();
            reg.emplace<Transform>(enemy, x, y);
            reg.emplace<Velocity>(enemy, dx, 0.0f, speed);
            reg.emplace<AnimationState>(enemy, 0, (int)enemyWalkFrames.size(), 0.0f, 7.0f, true);
            reg.emplace<Renderable>(enemy, enemySheet->GetSurface(), enemyWalkFrames, false);
            reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
            reg.emplace<EnemyTag>(enemy);
        };

        if (!mLevelPath.empty() && !mLevel.enemies.empty()) {
            for (const auto& e : mLevel.enemies)
                spawnEnemy(e.x, e.y, e.speed);
        } else {
            for (int i = 0; i < GRAVITYSLUGSCOUNT; ++i) {
                float x     = static_cast<float>(rand() % (mWindow->GetWidth() - 100));
                float y     = static_cast<float>(rand() % (mWindow->GetHeight() - SLIME_SPRITE_HEIGHT));
                float speed = 60.0f + static_cast<float>(rand() % 120);
                spawnEnemy(x, y, speed);
            }
        }
    }

    void Respawn() {
        reg.clear();
        gameOver           = false;
        levelComplete      = false;
        levelCompleteTimer = 2.0f;
        coinCount          = 0;
        stompCount         = 0;
        Spawn();
    }
};
