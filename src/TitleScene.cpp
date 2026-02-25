#include "TitleScene.hpp"
#include "GameScene.hpp"

std::unique_ptr<Scene> TitleScene::NextScene() {
    if (startGame) {
        startGame = false;
        return std::make_unique<GameScene>();
    }
    return nullptr;
}
