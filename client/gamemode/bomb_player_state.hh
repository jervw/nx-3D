#ifndef NX3D_CLIENT_GAMEMODE_BOMB_PLAYER_STATE_HH
#define NX3D_CLIENT_GAMEMODE_BOMB_PLAYER_STATE_HH

#include <cstdint>

namespace nx3d::client::gamemode
{

/// Before round one, an admitted player is active but has not spawned yet.
/// That is a waiting state, not an elimination.
constexpr bool shouldShowEliminated(uint64_t round, bool active, bool alive)
{
    return round > 0 && active && !alive;
}

} // namespace nx3d::client::gamemode

#endif
