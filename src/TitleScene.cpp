#include "TitleScene.hpp"
#include "GameScene.hpp"
#include "LevelEditorScene.hpp"

std::unique_ptr<Scene> TitleScene::NextScene() {
    if (startGame) {
        startGame = false;
        return std::make_unique<GameScene>(mChosenLevel);
    }
    if (openEditor) {
        openEditor = false;
        return std::make_unique<LevelEditorScene>();
    }
    return nullptr;
}
