# engine/

This directory is the destination for all engine-layer headers.
Nothing in here should know about game-specific concepts like
coins, enemies, health values, or level names.

## Headers that belong here (move gradually):
- Scene.hpp
- SceneManager.hpp
- Window.hpp
- Components.hpp  (core structs: Transform, Velocity, Collider, Renderable, etc.)
- Systems.hpp     (aggregator)
- systems/AnimationSystem.hpp
- systems/RenderSystem.hpp
- systems/BoundsSystem.hpp
- SpriteSheet.hpp
- Image.hpp
- Text.hpp
- Rectangle.hpp
- ErrorHandling.hpp
- SurfaceUtils.hpp
- Level.hpp
- LevelSerializer.hpp

## Rule: engine/ headers MUST NOT include game/ headers.
