#include <server/gamemode/game_mode_registry.hh>

#include <server/gamemode/bomb_mode.hh>
#include <server/gamemode/deathmatch_mode.hh>
#include <shared/fall_death.hh>

#include <nexilis/command_type.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/room_storage.hh>

#include <boost/json/value.hpp>

#include <map>
#include <utility>

namespace nx3d::server::gamemode
{

std::unordered_map<uint64_t, std::unique_ptr<GameMode>> GameModeRegistry::m_modes;
std::unordered_map<uint64_t, std::unordered_set<uint64_t>> GameModeRegistry::m_fallenPlayers;

namespace
{
std::unique_ptr<GameMode> createGameMode(nx3d::GameMode gameMode)
{
    switch (gameMode)
    {
        case nx3d::GameMode::deathmatch:
            return std::make_unique<DeathmatchMode>();
        case nx3d::GameMode::bomb:
            return std::make_unique<BombMode>();

        // Not implemented yet. Fall back to deathmatch so a room requesting
        // one of these still runs (with automatic respawns) until the mode is
        // actually built.
        case nx3d::GameMode::nospawn:
        case nx3d::GameMode::hardpoint:
        case nx3d::GameMode::custom:
            nexilis::Log::warning("Gamemode not implemented, falling back to deathmatch");
            return std::make_unique<DeathmatchMode>();
    }
    return std::make_unique<DeathmatchMode>();
}

} // namespace

void GameModeRegistry::assignRoom(nexilis::server::Room& room, nx3d::GameMode gameMode)
{
    m_fallenPlayers.erase(room.getId());
    auto mode = createGameMode(gameMode);
    auto* modePtr = mode.get();

    room.setDeathHandler([modePtr](nexilis::server::Room& r, uint64_t killerId, uint64_t victimId)
                         { modePtr->onPlayerDeath(r, killerId, victimId); });
    room.setTeamHandler([modePtr](nexilis::server::Room& r, uint64_t playerId)
                        { modePtr->onTeamSelected(r, playerId); });
    room.setActionHandler([modePtr](nexilis::server::Room& r, uint64_t playerId, uint8_t action)
                          { return modePtr->onAction(r, playerId, action); });
    room.setCombatHandler([modePtr](uint64_t shooterId, uint64_t targetId)
                          { return modePtr->canDamage(shooterId, targetId); });
    room.setTeamSelectionHandler([modePtr](uint64_t playerId)
                                 { return modePtr->canSelectTeam(playerId); });

    m_modes[room.getId()] = std::move(mode);
}

void GameModeRegistry::update(float dt)
{
    for (auto& [roomId, mode] : m_modes)
    {
        if (auto* room = nexilis::server::RoomStorage::getRoomById(roomId))
        {
            auto& fallen = m_fallenPlayers[roomId];
            for (auto playerId : room->getClients())
            {
                auto* player = nexilis::server::ClientStorage::getClientById(playerId);
                if (!player)
                    continue;
                if (player->getObject3D().getPosition().y >= nx3d::kFallDeathY)
                {
                    fallen.erase(playerId);
                    continue;
                }
                if (room->getPlayerTeam(playerId).empty() ||
                    room->getPlayerHealth(playerId) <= 0.0f || !fallen.insert(playerId).second)
                    continue;

                const float health = room->getPlayerHealth(playerId);
                if (!room->damagePlayer(playerId, health))
                    continue;

                // Use the usual damage event so the victim's HUD updates in
                // modes that leave dead players waiting for a new round.
                nexilis::nx_data damageData{
                        static_cast<uint8_t>(nexilis::CommandType::room),
                        static_cast<uint8_t>(nexilis::RoomCommandType::Root::player_3D),
                        static_cast<uint8_t>(nexilis::RoomCommandType::PlayerType::shoot)};
                std::map<std::string, boost::json::value> params{
                        {"target_id", boost::json::value(playerId)},
                        {"damage", boost::json::value(static_cast<double>(health))},
                        {"new_health", boost::json::value(0.0)}};
                room->broadcastToAll(nexilis::server::Command::createRoomCommand(
                        roomId, *player, damageData, params, 0));
                room->onPlayerDied(0, playerId);
            }
            mode->update(*room, dt);
        }
    }
}

void GameModeRegistry::clear()
{
    m_modes.clear();
    m_fallenPlayers.clear();
}

} // namespace nx3d::server::gamemode
