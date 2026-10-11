#ifndef NX3D_SERVER_GAMEMODE_DEATHMATCH_MODE_HH
#define NX3D_SERVER_GAMEMODE_DEATHMATCH_MODE_HH

#include <server/gamemode/game_mode.hh>

#include <mutex>
#include <unordered_map>

namespace nx3d::server::gamemode
{

/// Classic deathmatch.
///
/// Players respawn after a short death screen.
class DeathmatchMode : public GameMode
{
public:
    nx3d::GameMode type() const override;
    void onPlayerDeath(nexilis::server::Room& room, uint64_t killerId, uint64_t victimId) override;
    void update(nexilis::server::Room& room, float dt) override;

private:
    std::mutex m_respawnMutex;
    std::unordered_map<uint64_t, float> m_respawnTimers;
};

} // namespace nx3d::server::gamemode

#endif
