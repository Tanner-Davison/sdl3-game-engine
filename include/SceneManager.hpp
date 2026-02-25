#pragma once
#include "Scene.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <memory>

class SceneManager {
  public:
    void SetScene(std::unique_ptr<Scene> scene, Window& window) {
        if (mCurrent) mCurrent->Unload();
        mCurrent = std::move(scene);
        mCurrent->Load(window);
    }

    bool HandleEvent(SDL_Event& e) {
        if (!mCurrent) return false;
        return mCurrent->HandleEvent(e);
    }

    void Update(float dt, Window& window) {
        if (!mCurrent) return;
        mCurrent->Update(dt);

        // Check if the scene wants to transition
        auto next = mCurrent->NextScene();
        if (next) {
            mCurrent->Unload();
            mCurrent = std::move(next);
            mCurrent->Load(window);
        }
    }

    void Render(Window& window) {
        if (mCurrent) mCurrent->Render(window);
    }

    bool ShouldQuit() const {
        return mCurrent ? mCurrent->ShouldQuit() : true;
    }

  private:
    std::unique_ptr<Scene> mCurrent;
};
