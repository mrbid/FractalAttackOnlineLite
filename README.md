# FractalAttackOnline
### Defend the planet until it reaches 100% damage!
*Play with upto 8 players online.*

This is a low bandwidth implementation that attempts to setup the same initial game state at the same epoch across all clients using a custom pseudo random generator for a deterministic simulation.

This means that players only need to transmit their position. Players don't have a facing direction, but if they did I would infer it from their last position and their new position.

The PHP server is [fat.php](fat.php).

To start an online game you have to launch as such `./fat <msaa> <start epoch>` the start epoch has to be a future epoch, you can get the current epoch using `date +%s` add 180 seconds to it and tell your friends to also launch using that epoch and you will all endup in the same game. msaa is optional between 0-16.

Removing players bouncing off asteroids would probably completely eliminate any failed asteroid collisions across all clients, but it's less cool if players dont collide with asteroids and the current failed collision rate isn't that high. The result would be that players pass through asteroids to destroy them which would give all clients more time to detect their position within the asteroid radius than if they quickly bounced off them as they do now.

Ultimately IPv4 UDP at 144hz is the holy grail. 🙇

### If you can't ping the server in under 16ms then you won't be able to play basically

#### Single Player version
- https://snapcraft.io/fractalattack
- https://github.com/mrbid/FractalAttack

---

<details>
    <summary>Server</summary>

### Server

Server should only track 8 player positions per game id.

- **Register** `?r=<game id>`
  
`game-id` is the unix epoch of when the game starts, after this epoch passes no more registrations should be accepted for that epoch/game-id. The server should assign each registration session with its `game-id`, preferably as session data. Registrations only require a 200 OK response.
- **Position** `?p=%00%00%00%00%01%01%01%01%02%02%02%02`
  
When each player submits a position they will do so via the `p` url parameter as bytecode provided as escaped hex, it will be 3 floats of 4 bytes each. The server should then respond with an array of 7 player positions as bytecode in any order as long as they are all of the same `game-id` and excluding the position of the player that made the request.

All you really need to do is update the player position bytecode on `p`, you don't need to format it just store it, and then just spit them back out with all the other player position bytecodes in one long string of bytecodes but remembering to exclude the players own position bytecode.
</details>

---

### Attributions
https://www.solarsystemscope.com/textures/
- https://www.solarsystemscope.com/textures/download/4k_eris_fictional.jpg
- https://www.solarsystemscope.com/textures/download/8k_venus_surface.jpg
