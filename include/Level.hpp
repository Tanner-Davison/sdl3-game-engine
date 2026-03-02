#pragma once
#include <string>
#include <vector>

// Plain data — one entry per entity type in the level
struct CoinSpawn {
    float x, y;
};
struct EnemySpawn {
    float x, y;
    float speed;
};
struct PlayerSpawn {
    float x, y;
};
struct TileSpawn {
    float       x, y;
    int         w, h;
    std::string imagePath;
};

struct Level {
    std::string             name       = "Untitled";
    std::string             background = "game_assets/backgrounds/deepspace_scene.png";
    PlayerSpawn             player     = {0.0f, 0.0f};
    std::vector<CoinSpawn>  coins;
    std::vector<EnemySpawn> enemies;
    std::vector<TileSpawn>  tiles;
};
