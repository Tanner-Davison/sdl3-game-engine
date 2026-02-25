#pragma once
#include <SDL3/SDL.h>

class Window;

class Scene {
  public:
    virtual ~Scene() = default;

    // Called once when the scene becomes active
    virtual void Load(Window& window) = 0;

    // Called once when the scene is replaced or popped
    virtual void Unload() = 0;

    // Handle a single SDL event — return false to quit the app
    virtual bool HandleEvent(SDL_Event& e) = 0;

    // Update game logic
    virtual void Update(float dt) = 0;

    // Render to window surface
    virtual void Render(Window& window) = 0;

    // If this returns non-null, SceneManager will switch to it next frame
    virtual std::unique_ptr<Scene> NextScene() { return nullptr; }

    // If this returns true, SceneManager will quit the app
    virtual bool ShouldQuit() const { return false; }
};
