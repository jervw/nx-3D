#include <server/gamemode/game_mode_registry.hh>
#include <shared/economy.hh>
#include <shared/bomb_sites.hh>

#include <nexilis/client/client_api.hh>
#include <nexilis/command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/room_storage.hh>

#include <stdexcept>
#include <string>

namespace
{
void check(bool ok, const char* reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
} // namespace

int main()
{
    using namespace nexilis;
    using nx3d::server::gamemode::GameModeRegistry;
    client::ClientConfig config;
    client::ClientAPI t(config), ct(config), queued(config);
    t.setClientId(10);
    ct.setClientId(20);
    queued.setClientId(30);

    server::RoomStorage::add(server::Room(RoomData(10, "Bomb test", 777)));
    auto* room = server::RoomStorage::getRoomById(777);
    GameModeRegistry::assignRoom(*room, nx3d::GameMode::bomb);
    std::vector<std::pair<uint64_t, client::ClientAPI*>> presentClients;
    auto announce = [](client::ClientAPI& api, uint64_t id)
    {
        check(api.readMessage(server::Command::clientMessageData(
                      CommandType::room, "management", 0,
                      {{"action", "join"}, {"room_id", 777}, {"client_id", id},
                       {"username", boost::json::value("player_" + std::to_string(id))}})) == client::ReadResult::success,
              "Test client could not register room player");
    };
    auto add = [&](uint64_t id, client::ClientAPI& api, const char* team)
    {
        check(api.readMessage(server::Command::clientMessageData(
                      CommandType::room, "management", 0,
                      {{"action", "create"}, {"room_id", 777}, {"client_id", id},
                       {"room_name", "Bomb test"}, {"room_context", 1}})) == client::ReadResult::success,
              "Test client could not join room messages");
        for (const auto& [otherId, otherApi] : presentClients)
        {
            announce(api, otherId);
            announce(*otherApi, id);
        }
        announce(api, id);
        presentClients.emplace_back(id, &api);
        auto user = std::make_unique<server::User>(id, "127.0.0.1");
        user->setRoomId(777);
        user->setUsername("player_" + std::to_string(id));
        user->setBoostTCPSend([&api](const nx_data& data)
                              { check(api.readMessage(data) == client::ReadResult::success, "Message rejected"); });
        server::ClientStorage::add(std::move(user));
        room->joinRoom(id);
        room->setPlayerTeam(id, team);
    };
    auto bomb = [](const std::vector<std::unique_ptr<client::ClientAPI::MatchEvent>>& events)
            -> const client::ClientAPI::BombMatchEvent&
    {
        check(!events.empty() && events.back() &&
                      events.back()->kind() == client::ClientAPI::MatchEvent::Kind::bomb,
              "Missing bomb match event");
        return static_cast<const client::ClientAPI::BombMatchEvent&>(*events.back());
    };
    add(10, t, "Terrorist");
    GameModeRegistry::update(1.0f);
    check(!room->canDamage(10, 20), "Round started with an empty Counter Terrorist team");
    auto waiting = t.consumeMatchEvents();
    check(bomb(waiting).notice ==
                  "Cannot start game because Counter Terrorist team is empty",
          "Missing-team error was not sent to clients");

    check(room->canSelectTeam(10), "Waiting player could not switch team");
    room->setPlayerTeam(10, "Counter Terrorist");
    GameModeRegistry::update(1.0f);
    auto waitingForT = t.consumeMatchEvents();
    check(bomb(waitingForT).notice ==
                  "Cannot start game because Terrorist team is empty",
          "Terrorist empty-team error was not sent to clients");
    room->setPlayerTeam(10, "Terrorist");

    add(20, ct, "Counter Terrorist");
    GameModeRegistry::update(1.0f);
    check(room->canDamage(10, 20), "Round failed to start");
    auto first = t.consumeMatchEvents();
    check(bomb(first).round == 1 && bomb(first).alive, "Bad first round state");

    auto* ctUser = server::ClientStorage::getClientById(20);
    ctUser->getObject3D().setPosition(Vector3f{355.0f, -48.0f, -2364.0f});
    check(room->onPlayerAction(20, 16), "CT could not buy a defuse kit at spawn");
    nx3d::economy::Snapshot kitState;
    check(nx3d::economy::decode(ct.getChatSnapshot(777).back().message, kitState) &&
                  kitState.money == 400 && kitState.hasDefuseKit,
          "Defuse kit purchase did not debit money or grant equipment");
    ctUser->getObject3D().setPosition(Vector3f{0.0f, 0.0f, 0.0f});
    check(!room->onPlayerAction(20, 12), "Purchase outside spawn was accepted");
    auto* tUser = server::ClientStorage::getClientById(10);
    tUser->getObject3D().setPosition(Vector3f{-592.0f, 192.0f, 764.0f});
    check(!room->onPlayerAction(10, 16), "Terrorist bought a defuse kit");
    check(room->onPlayerAction(10, 20), "Terrorist could not buy smoke");
    check(room->onPlayerAction(10, nx3d::economy::kThrowSmokeAction),
          "Smoke throw was rejected");
    nx3d::economy::GrenadeThrow smoke;
    check(nx3d::economy::decodeGrenade(t.getChatSnapshot(777).back().message, smoke) &&
                  smoke.smoke && smoke.owner == 10,
          "Smoke event was not broadcast with its type");
    check(!room->onPlayerAction(10, nx3d::economy::kThrowSmokeAction),
          "Smoke could be thrown after inventory ran out");
    ctUser->getObject3D().setPosition(Vector3f{smoke.endX, 192.0f, smoke.endZ});
    GameModeRegistry::update(2.1f);
    check(room->getPlayerHealth(20) == 100.0f, "Smoke damaged a player");
    ctUser->getObject3D().setPosition(Vector3f{0.0f, 0.0f, 0.0f});
    tUser->getObject3D().setPosition(Vector3f{0.0f, 0.0f, 0.0f});
    check(nx3d::bomb_sites::at(1152.0f, 160.0f, -2416.0f) &&
                  nx3d::bomb_sites::at(-1600.0f, 64.0f, -2700.0f) &&
                  !nx3d::bomb_sites::at(0.0f, 0.0f, 0.0f),
          "Bombsite bounds do not contain both map site markers");

    add(30, queued, "Terrorist");
    check(!room->canDamage(30, 20), "Queued player could shoot");
    auto queueEvents = queued.consumeMatchEvents();
    check(bomb(queueEvents).queue_position == 1, "Queue position missing");
    const auto queuedPosition = server::ClientStorage::getClientById(30)->getObject3D().getPosition();
    check(room->onMovementInput(30, Vector3f{1.0f, 0.0f, 0.0f}, 0.0f),
          "Queued player's movement packet was reported as invalid");
    GameModeRegistry::update(0.05f);
    check(server::ClientStorage::getClientById(30)->getObject3D().getPosition().x == queuedPosition.x,
          "Queued player moved before being admitted");

    check(!room->onPlayerAction(10, 1), "Plant outside a bombsite was accepted");
    tUser->getObject3D().setPosition(Vector3f{1152.0f, 160.0f, -2416.0f});
    check(room->onPlayerAction(10, 1), "Plant at A was rejected");
    GameModeRegistry::update(1.5f);
    tUser->getObject3D().setPosition(Vector3f{0.0f, 0.0f, 0.0f});
    GameModeRegistry::update(1.7f);
    auto interrupted = t.consumeMatchEvents();
    check(!bomb(interrupted).bomb_planted, "Plant continued after leaving A");
    tUser->getObject3D().setPosition(Vector3f{1152.0f, 160.0f, -2416.0f});
    check(room->onPlayerAction(10, 1), "Plant restart at A was rejected");
    GameModeRegistry::update(3.1f);
    auto planted = t.consumeMatchEvents();
    check(bomb(planted).bomb_planted && bomb(planted).bomb_x == 1152.0f,
          "Bomb did not plant at A");
    ctUser->getObject3D().setPosition(Vector3f{1152.0f, 160.0f, -2416.0f});
    check(room->onPlayerAction(20, 2), "Defuse rejected");
    GameModeRegistry::update(2.6f);
    auto defused = ct.consumeMatchEvents();
    check(bomb(defused).counter_terrorist_score == 1,
          "Defuse kit did not shorten the defuse and score");

    GameModeRegistry::update(3.9f);
    check(!room->canDamage(30, 20), "Next round began too early");
    GameModeRegistry::update(0.2f);
    check(room->canDamage(30, 20), "Queued player not admitted next round");
    check(room->damagePlayer(20, 100.0f), "Kill did not lower health");
    room->onPlayerDied(10, 20);
    auto eliminated = t.consumeMatchEvents();
    check(bomb(eliminated).terrorist_score == 1,
          "Elimination did not score");
    check(!room->canDamage(10, 20), "Post-round shooting allowed");

    // Finish the first half, then verify the MR12 side swap and score mapping.
    for (int round = 3; round <= 12; ++round)
    {
        GameModeRegistry::update(4.1f);
        check(room->damagePlayer(20, 100.0f), "Round victim was not revived");
        room->onPlayerDied(10, 20);
    }
    GameModeRegistry::update(4.1f);
    check(room->getPlayerTeam(10) == "Counter Terrorist" &&
                  room->getPlayerTeam(20) == "Terrorist",
          "Halftime did not swap sides");
    auto secondHalf = ct.consumeMatchEvents();
    check(bomb(secondHalf).round == 13 &&
                  bomb(secondHalf).counter_terrorist_score == 11,
          "Half-time score was not carried to new sides");
    check(room->canSelectTeam(10), "Active player could not switch team");

    auto* secondT = server::ClientStorage::getClientById(20);
    secondT->getObject3D().setPosition(Vector3f{-1600.0f, 64.0f, -2700.0f});
    check(room->onPlayerAction(20, 1), "Second-half plant at B rejected");
    GameModeRegistry::update(3.1f);
    GameModeRegistry::update(40.1f);
    auto exploded = t.consumeMatchEvents();
    check(bomb(exploded).terrorist_score == 2,
          "Explosion did not score");

    GameModeRegistry::clear();
    server::RoomStorage::clear();
    server::ClientStorage::clear();
}
