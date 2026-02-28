
#include "LevelTwo.hpp"
#include "LevelThree.hpp"

std::unique_ptr<Scene> LevelTwo::NextScene() {
    if (levelComplete && levelCompleteTimer <= 0.0f)
        return std::make_unique<LevelThree>();
    return nullptr;
}
