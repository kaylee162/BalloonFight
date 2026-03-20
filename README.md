# Balloon Fight

A two-level, side-scrolling GBA platformer inspired by classic balloon-style arcade movement. The player floats through each level, avoids enemies, collects balloons, and advances by clearing objectives. The game was built in Mode 0 and uses custom-made visual assets, sprites, and state screens.

---

## Game Overview

In **Balloon Fight**, the player moves through floating platform levels while managing movement, avoiding enemies, and collecting balloons. The game has two levels with different layouts and traversal challenges.

In **Level 1**, the player must defeat all enemies before a door appears. Once the enemies are gone, the player can enter the door to advance.

In **Level 2**, the focus shifts more toward traversal and item collection. The player must explore the larger map, collect balloons, and navigate around obstacles and platforms.

The game includes sound effects, a scoreboard system, animated state screens, and custom sprite work.

---

## How to Play

The goal of the game is to survive, move carefully through each level, and complete each level’s objective.

### Level 1
- Move through the level while avoiding or attacking enemies.
- Enemies begin in a floating phase and can attack the player.
- Shoot enemies to weaken or eliminate them.
- Once all enemies are defeated, a **door appears**.
- Touch the door to open it, then enter to transition to the next level.

### Level 2
- Explore the larger scrolling level.
- Collect balloons placed throughout the map.
- Use floating movement to reach higher or more difficult areas.
- Avoid hazards and navigate platforms carefully.
- Finish the level by collecting the required items.

### Lives and Balloon Mechanics
- The player’s floating ability is tied to remaining lives.
- Although the original inspiration involves visible balloons attached to the player, I chose **not** to visually attach balloons to the player because it looked awkward and cluttered in motion.
- The required gameplay effect is still present: the player’s movement and floating behavior are still tied to their remaining lives.

---

## Controls

- **D-Pad Left / Right**: Move horizontally
- **Button A**: Float upward
- **Button B**: Shoot bullets
- **Start**: Start the game / progress from game state screens

### Movement Notes
- The player is always gradually falling unless supported by a platform or actively floating upward.
- Floating is used both for traversal and for avoiding hazards/enemies.

---

## Enemy and Balloon / Item Layout

### Enemies
Enemies are primarily featured in **Level 1**.

- Enemies begin in a **floating phase**, where they can move through the air and shoot at the player.
- If the player shoots an enemy, it transitions into a lower phase where it drops toward the nearest platform/ground.
- After that, the enemy becomes easier to eliminate.

### Enemy Traversal Behavior
- When enemies are shot in Level 1, they move downward toward the nearest platform.
- After landing, they are **not strictly bound to the platform** and may continue walking off the edge.
- This behavior was intentional. I felt it created more natural and dynamic gameplay rather than making enemies feel locked to a rigid path.

### Balloons / In-Game Items
Balloons are the primary collectible item.

- Balloons are placed throughout the level to encourage movement across the map.
- Their positions were chosen to make the player engage with vertical movement, timing, and full-map traversal.
- Some balloons are placed in easier low-risk positions, while others require traveling farther across platforms or reaching more difficult vertical spaces.

### How Balloons Affect Traversal
- Balloons are not just collectibles. Their placement guides the player through the level.
- In Level 2 especially, balloon placement encourages the player to explore the larger map and use both horizontal and vertical movement.
- Balloons help pace movement by pulling the player into different areas of the level rather than letting them stay in one safe zone.

---

## Level Design Notes

### Level 1
- Smaller than Level 2, but still uses scrolling and camera tracking.
- Includes enemies, balloons, and a door transition system.
- The door that appears after defeating all enemies is **animated**, which adds visual feedback and a more polished level transition.

### Level 2
- Larger map with more traversal emphasis.
- Designed to make the player scroll through more of the world and interact more heavily with platform layout and item placement.
- Uses both horizontal and vertical movement more heavily than Level 1.

---

## Technical Setup

- Built for the **Game Boy Advance**
- Implemented in **Mode 0**
- Uses tilemaps, sprites, and background layers
- Includes horizontal and vertical scrolling
- Uses DMA for graphics transfer
- Includes sound support
- Includes scoreboard / HUD logic

### Asset Creation
The following assets were created by hand and are my original work:
- Tileset
- Tilemaps
- Sprite sheet
- State screen backgrounds

All artwork in the project was drawn by me.

---

## Extra Credit / Notes for Grading

The following features were added beyond the basic core gameplay:

### Extra Credit Features
- **Sound support**
- **Scoreboard system**
  - High score display
  - Current score tracking
- **Scrolling Level 1 background**
  - Even though Level 1 is smaller than Level 2, it still uses camera tracking, `hOff`, and `vOff`
- **Animated door**
  - After all enemies are defeated, the door appears and animates open when the player collides with it

### Additional Notes for Consideration
- I chose not to visually attach balloons to the player because the effect looked very awkward in-game, but the gameplay behavior tied to lives and floating is still implemented.
- Enemy post-hit movement was intentionally designed to feel dynamic rather than heavily constrained.
- All main visual assets were original and hand-created by me.

---

## Known Issues

- There is still a transition glitch between **Level 1 and Level 2** related to drawing/loading the new background cleanly.
- There may also be occasional visual issues tied to sprite/background transitions depending on when the level changes occur.

These were actively being debugged, but the core gameplay systems are implemented and functional.

---

## Summary

This project focuses on:
- floating movement
- platform traversal
- enemy combat
- collectible-based progression
- scrolling level design
- original GBA art and presentation

It combines arcade-style action with traversal-focused gameplay and includes several polish features such as sound, animation, and a scoreboard system.