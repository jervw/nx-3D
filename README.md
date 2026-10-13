# nx-3D

nx-3D is a multiplayer first-person shooter with a dedicated server and a Vulkan client. The server offers a bomb match and team deathmatch on de_dust2. Players choose Counter Terrorist or Terrorist and spawn with an M16 or AK-47 respectively. Weapons can be dropped and picked up from the map.

## Requirements

- Linux with a Vulkan-capable graphics driver and SDL3 audio support.
- CMake 3.30 or newer and a C++20 compiler.
- The public `spear` and `nexilis` Git submodules.
- The libraries used by `spear` and `nexilis`. The Nix development shell in `flake.nix` supplies these dependencies.

Clone with submodules:

```sh
git clone --recurse-submodules <repository-url>
cd nx-3D
```

## Build and run

On Nix-enabled Linux, enter the development shell with `nix develop`. With the dependencies available, build both programs and run the regression checks:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The default build compiles Nexilis from the checked-out submodule in the same build as its consumers. Rebuild after updating submodules: newer Nexilis headers with an older `libnexilis.so` can corrupt the client event queues and crash on damage/death messages. Restart all running clients and servers after rebuilding.

Standalone `cmake -S client -B client/build` and `cmake -S server -B server/build` builds also compile their own matching Nexilis library. For a server-only root build, pass `-DNX3D_BUILD_CLIENT=OFF`; disable regression tests with `-DBUILD_TESTING=OFF` if GLM is unavailable. Packagers can opt into an external package with `-DNX3D_USE_SYSTEM_NEXILIS=ON -DNexilis_DIR=/path/to/package`, provided its headers and library match. The Nix packages use this explicit external-package mode.

Run the server first, then the client in another terminal:

```sh
./build/server/nx-3D-server
./build/client/nx_3D_client
```

The default network ports remain TCP 54200 for authentication, TCP 54202 for the game session, and UDP 54201 for gameplay. To run a server on different ports, pass all three values:

```sh
./build/server/nx-3D-server \
  --tcp-port 55200 \
  --tcp-session-port 55202 \
  --udp-port 55201
```

Clients connecting to that server need its authentication TCP and UDP ports. The session TCP port does not need a client argument because the server sends it to the client during authentication:

```sh
./build/client/nx_3D_client \
  --server game.example.com \
  --username player \
  --tcp-port 55200 \
  --udp-port 55201
```

Each port option is optional. An omitted option keeps its default value, so the existing commands continue to work unchanged. Ports must be numbers from 1 through 65535. The single-dash forms (`-tcp-port`, `-tcp-session-port`, and `-udp-port`) are also accepted for consistency with the existing client arguments.

To run only the server with Docker on Linux, initialize the Nexilis submodule and start Compose:

```sh
git submodule update --init --recursive nexilis
docker compose up --build -d
```

Use `docker compose logs -f server` to view its output and `docker compose down` to stop it. Compose uses host networking on Linux. Remote clients need the configured authentication TCP, session TCP, and UDP ports. Allow these ports through the host firewall and forward them on the router when hosting behind NAT. The client accepts an IPv4 address or hostname in `-server`. The server currently has hardcoded credentials (`password` for players and `root` for administration); change them in `server/server.cc` before exposing it publicly.

The client initially asks for a server address and username. Enter an IPv4 address or domain name (for example, `game.example.com`); the default is `127.0.0.1`. You can also start it with `-server <address> -username <name>` to connect immediately. Select a room with the arrow keys and Enter, then click a team to join.

The flake also exposes packaged programs through `nix run .#server` and `nix run .#client`. Its Nexilis input uses SSH access to the private repository.

## Controls

| Action                            | Control                                   |
| --------------------------------- | ----------------------------------------- |
| Move and look                     | WASD and mouse                            |
| Jump                              | Space                                     |
| Fire                              | Hold left mouse button                    |
| Reload                            | R                                         |
| Plant or defuse bomb (bomb match) | Hold E                                    |
| Throw held weapon onto the ground | G                                         |
| Pick up a nearby weapon           | F                                         |
| Change team                       | Escape, then click CT or T                |
| Leaderboard                       | Hold Tab                                  |
| Chat                              | Y, then Enter to send or Escape to cancel |
| Pause or resume                   | Escape                                    |

The equipped gun appears beside the camera and follows the view. It has walk bob and recoil. The gun mesh is selected from the team's weapon profile or the weapon picked up from the ground. First-person placement and animation live in `client/gun/first_person_gun.cc`; per-weapon mesh paths, scale, and center are in `client/gamemode/weapon_profiles.hh`.

The AK-47 and M16 are original low-poly models in `assets/weapons/`. Run
`python3 assets/weapons/generate.py` to regenerate their OBJ and MTL files.
The AK-47 uses a black polymer and metal palette; the M16 uses charcoal and
gunmetal. Their materials use generated 1-pixel swatch textures.

Other players appear as custom low-poly OBJ figures: a blue armored Counter Terrorist and a tan Terrorist with a head wrap and utility vest. The figures and their texture atlases live in `assets/players/`; see its README for the generator and dimensions. Figures update when a player's team becomes known or changes. The client-side shot box spans the figures from headgear to boots.

Both rifles hold 30 shots. R reloads a partly empty magazine in 2.5 seconds; firing is disabled while reloading. The held rifle dips and tilts during the reload, then returns to its ready position. The weapon HUD shows remaining ammunition and reload status. Ammunition is currently managed by the client, matching the existing client-reported hit model. Dead players see a red and black WASTED screen and cannot move or turn until respawning. Deathmatch respawns take 2 seconds; bomb rounds have a 4-second break. Fallen player models tumble under Bullet physics. A respawn or new bomb round refills the magazine.

Reload and planting sounds play locally and are relayed to nearby players through Nexilis positional audio. A bomb explosion sound plays for everyone when the server declares an explosion win. The three original PCM assets are `assets/sounds/reload.wav`, `planting.wav`, and `explosion.wav`; regenerate them with `python3 assets/sounds/generate.py`.

Gunshots use a louder volume curve and a 2500-unit spatial range so distant firing remains audible across much of the map while fading with distance. The volume control still mutes all sounds at zero.

Weapons render in camera space to avoid shaking from world-coordinate rounding. Their default pose sits toward the lower right at an angled FPS stance. `client/gun/weapon_motion.hh` controls the subtle walking cadence, time-based recoil recovery, and reload motion; vertical movement, small ground-contact jitter, and respawn teleports do not drive the bob animation. G tosses the current rifle ahead as a physics-driven pickup. Press F near a dropped rifle to equip it; it stays on the ground until picked up.

## Bomb match rules and queue

The Bomb Match room starts when at least one player has chosen each team. If either team is empty, every client sees which team is preventing the game from starting. Players may switch teams from the Escape menu, with no team size or balance restriction. A player who joins during a live round watches until the next one. The HUD shows queue position, score, round, time, life state, and bomb state. Deaths have no immediate respawn. Only living participants can shoot, plant, or defuse. Leaving the room removes a player from the match or queue.

The match uses MR12: twelve rounds per half, side swap after round 12, first to 13 rounds wins, and a 12–12 score after round 24 is a draw. A round lasts 115 seconds. The Terrorists win by eliminating all Counter Terrorists or by letting a planted bomb explode. Counter Terrorists win by eliminating all Terrorists before the bomb is planted, defusing it, or surviving until time expires before a plant. If all Terrorists die after planting, the bomb still needs to be defused. There are five seconds between rounds; a completed match restarts after ten seconds.

The first version uses one shared bomb: any living Terrorist may hold E for three seconds to plant at their current position. A Counter Terrorist within 120 game units of the planted bomb may hold E for five seconds to defuse. Releasing E cancels the action. The bomb explodes 40 seconds after planting. Its current placeholder is a red cube at the planted position, with a square bomb indicator in the HUD; there is no site restriction, dropped bomb, economy, freeze time, or overtime yet. The server checks team, life state, phase, and defuse range, but player positions and hit targets still come from clients.

## Manual releases

Commit and push the default branch, then run **Game release** from the GitHub Actions tab or use `gh workflow run release.yml`. Leave the version input blank to increment the patch version from `VERSION.txt`, or enter a newer version such as `0.2.0`. Watch the run with `gh run list --workflow release.yml`.

After all builds succeed, the workflow updates `VERSION.txt` and the package versions in `flake.nix`, tags the release, and publishes five GitHub release assets: Linux and Windows archives for both server and client, plus a source archive. The source archive contains this repository's tracked files and submodule references; clone the tagged repository with submodules to build from source. Linux clients run `run.sh` from the extracted archive; Windows clients run `run.bat`. The release workflow has no schedule.

## Verification and release status

The deathmatch regression runs the game's server death handler through Nexilis and delivers the resulting damage, respawn, and leaderboard messages to two client parsers for 100 consecutive kills. The bomb regression covers admission from the queue, spectator combat restrictions, plant, defuse, elimination, and round score. The animation regression checks idle settling, teleport/airborne suppression, and recoil recovery at 30, 60, and 144 FPS. These tests run without a window or live network sockets.

Before a public release, exercise multiplayer sessions on real Vulkan hardware, including disconnects during combat, repeated weapon drops/pickups, and extended matches. The current game still accepts client-reported hits/positions and uses hardcoded server credentials; server-authoritative hit validation and deployment configuration remain release work. Passing these regressions does not establish production readiness.

## Project layout

| Path                              | Purpose                                                              |
| --------------------------------- | -------------------------------------------------------------------- |
| `client/app/`                     | Client loop, rendering, networking, input, and UI state              |
| `client/gamemode/`                | Bomb and deathmatch presentation, weapon handling, and profiles      |
| `client/gun/`                     | First-person weapon rendering and animation                          |
| `client/crosshair/`, `client/ui/` | Crosshair and menus/HUD                                              |
| `server/`                         | Dedicated server, rooms, and gun logic                               |
| `shared/`                         | Types shared by client and server                                    |
| `assets/`                         | Map, gun models, textures, fonts, and sounds                         |
| `tests/`, `cmake/`                | Gameplay regression checks and shared dependency build configuration |
| `spear/`, `nexilis/`              | Engine and networking submodules                                     |

The server creates one bomb room and one deathmatch room. The client uses Nexilis for room and player synchronization, while `spear` supplies Vulkan rendering, physics, input, and audio. Client assets are resolved relative to the repository root compiled into `PROJECT_ROOT` by CMake.
