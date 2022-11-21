# FractalAttackOnline
An online version of Fractal Attack.

This is a low bandwidth implementation that attempts to setup the same initial game state at the same epoch across all clients using a pseudo random generator for a deterministic simulation.

This means that players only need to transmit their position. Players don't have a facing direction, but if they did I would infer it from their last position and their new position.

The Asteroids move quite slowly so being even a second or two out of sync would not be that bad, but reducing the speed even more compensates for higher deviations between clients.

Here's the fun part, it's a make your own server game *(for now)*. It's not that hard, have fun!

#### Single Player version
- https://snapcraft.io/fractalattack
- https://github.com/mrbid/FractalAttack

### Attributions
https://www.solarsystemscope.com/textures/
- https://www.solarsystemscope.com/textures/download/4k_eris_fictional.jpg
- https://www.solarsystemscope.com/textures/download/8k_venus_surface.jpg
