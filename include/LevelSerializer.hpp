#pragma once
#include "Level.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <print>
#include <string>

using json = nlohmann::json;

inline bool SaveLevel(const Level& level, const std::string& path) {
    json j;
    j["name"]       = level.name;
    j["background"] = level.background;
    j["player"]     = {{"x", level.player.x}, {"y", level.player.y}};

    j["coins"] = json::array();
    for (const auto& c : level.coins)
        j["coins"].push_back({{"x", c.x}, {"y", c.y}});

    j["enemies"] = json::array();
    for (const auto& e : level.enemies)
        j["enemies"].push_back({{"x", e.x}, {"y", e.y}, {"speed", e.speed}});

    j["tiles"] = json::array();
    for (const auto& t : level.tiles)
        j["tiles"].push_back({{"x", t.x}, {"y", t.y}, {"w", t.w}, {"h", t.h}, {"img", t.imagePath}});

    std::ofstream file(path);
    if (!file.is_open()) {
        std::print("Failed to save level: {}\n", path);
        return false;
    }
    file << j.dump(4); // pretty print with 4-space indent
    std::print("Level saved: {}\n", path);
    return true;
}

inline bool LoadLevel(const std::string& path, Level& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::print("Failed to load level: {}\n", path);
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        std::print("JSON parse error in {}: {}\n", path, e.what());
        return false;
    }

    out.name       = j.value("name", "Untitled");
    out.background = j.value("background", "game_assets/base_pack/deepspace_scene.png");

    if (j.contains("player")) {
        out.player.x = j["player"].value("x", 0.0f);
        out.player.y = j["player"].value("y", 0.0f);
    }

    out.coins.clear();
    for (const auto& c : j.value("coins", json::array()))
        out.coins.push_back({c.value("x", 0.0f), c.value("y", 0.0f)});

    out.enemies.clear();
    for (const auto& e : j.value("enemies", json::array()))
        out.enemies.push_back({e.value("x", 0.0f), e.value("y", 0.0f), e.value("speed", 120.0f)});

    out.tiles.clear();
    for (const auto& t : j.value("tiles", json::array()))
        out.tiles.push_back({t.value("x", 0.0f), t.value("y", 0.0f),
                             t.value("w", 40), t.value("h", 40),
                             t.value("img", std::string{})});

    std::print("Level loaded: {} ({} coins, {} enemies)\n",
               out.name, out.coins.size(), out.enemies.size());
    return true;
}
