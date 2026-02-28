#include "GameScene.hpp"
#include "LevelTwo.hpp"

std::unique_ptr<Scene> GameScene::NextScene() {
    if (levelComplete && levelCompleteTimer <= 0.0f)
        return std::make_unique<LevelTwo>();
    return nullptr;
}
