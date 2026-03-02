# game/

This directory holds all game-specific headers that sit on top of the engine.

## Headers that belong here (already here or moving here):
- GameConfig.hpp   — all tuning constants (health, speed, counts, dimensions)
- GameEvents.hpp   — CollisionResult and any future event structs
- GameScene.hpp    — Level 1
- LevelTwo.hpp
- LevelThree.hpp
- TitleScene.hpp
- LevelEditorScene.hpp
- systems/CollisionSystem.hpp   (game-specific: knows about CoinTag, DeadTag, etc.)
- systems/InputSystem.hpp       (game-specific: hardcoded WASD controls)
- systems/MovementSystem.hpp    (game-specific: GravityDir multi-wall logic)
- systems/PlayerStateSystem.hpp
- systems/HUDSystem.hpp
- systems/CenterPullSystem.hpp

## Rule: game/ headers MAY include engine/ headers, never the reverse.
