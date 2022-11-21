# FractalAttackOnline
An online version of Fractal Attack.

This is a low bandwidth implementation that attempts to setup the same initial game state at the same epoch across all clients using a custom pseudo random generator for a deterministic simulation.

This means that players only need to transmit their position. Players don't have a facing direction, but if they did I would infer it from their last position and their new position.

The Asteroids move quite slowly so being even a second or two out of sync would not be that bad, but reducing the speed even more compensates for higher deviations between clients.

Here's the fun part, it's a make your own server game *(for now)*. It's not that hard, have fun!

#### Single Player version
- https://snapcraft.io/fractalattack
- https://github.com/mrbid/FractalAttack

---

### Server
Server should only track 8 player positions per game id.

- **Register** `?r=<game id>`
  
`game-id` is the unix epoch of when the game starts, after this epoch passes no more registrations should be accepted. The server should assign each registration it's unique index from 0-7 and save its `game-id`, preferably as session data.
- **Position** `?p=%00%00%00%00%01%01%01%01%02%02%02%02`
  
When each player submits a position they will do so via the `p` url parameter as bytecode provided as escaped hex, it will be 3 floats of 4 bytes each. The server should then respond with an array of 7 player positions as bytecode in their respective unique index order all of the same `game-id` excluding your own position.

---

### Attributions
https://www.solarsystemscope.com/textures/
- https://www.solarsystemscope.com/textures/download/4k_eris_fictional.jpg
- https://www.solarsystemscope.com/textures/download/8k_venus_surface.jpg
