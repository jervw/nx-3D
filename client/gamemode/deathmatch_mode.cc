#include <client/app/client_app.hh>
#include <client/gamemode/deathmatch_mode.hh>
#include <client/gamemode/player_hit_test.hh>
#include <client/gamemode/weapon_profiles.hh>

#include <nexilis/client/packet.hh>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace nx3d::client::gamemode
{

namespace blt = spear::physics::bullet;

namespace
{
/// Recoil ramps up as a burst goes on: each shot multiplies the previous
/// shot's view/gun punch slightly more, capped so sustained fire stays
/// controllable. Reset on trigger release.
constexpr float kRecoilStartMultiplier = 0.6f;
constexpr float kRecoilStep = 0.15f;
constexpr float kMaxRecoilMultiplier = 2.5f;

class MapRayResultCallback : public btCollisionWorld::ClosestRayResultCallback
{
public:
    MapRayResultCallback(const btVector3& from, const btVector3& to,
                         const btCollisionObject* mapBody)
        : btCollisionWorld::ClosestRayResultCallback(from, to),
          m_mapBody(mapBody)
    {
    }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& result,
                             bool normalInWorldSpace) override
    {
        if (result.m_collisionObject != m_mapBody)
            return 1.0f;
        return ClosestRayResultCallback::addSingleResult(result, normalInWorldSpace);
    }

private:
    const btCollisionObject* m_mapBody;
};

bool mapBlocksShot(const ClientApp& app, const glm::vec3& origin,
                   const glm::vec3& direction, float targetDistance)
{
    if (!app.mapCollisionBody || !app.shared_world || !std::isfinite(targetDistance))
        return false;

    const glm::vec3 target = origin + direction * targetDistance;
    const btVector3 from(origin.x, origin.y, origin.z);
    const btVector3 to(target.x, target.y, target.z);
    MapRayResultCallback hit(from, to, app.mapCollisionBody);
    app.shared_world->rayTest(from, to, hit);
    return hit.hasHit() && hit.m_closestHitFraction < 1.0f;
}
} // namespace

nx3d::GameMode DeathmatchMode::type() const
{
    return nx3d::GameMode::deathmatch;
}

std::string DeathmatchMode::playerTeam(uint64_t playerId) const
{
    const auto it = m_leaderboard.find(playerId);
    return it == m_leaderboard.end() ? std::string{} : it->second.team;
}

void DeathmatchMode::onEnter(ClientApp& app, const std::string& team)
{
    using packet = nexilis::client::Packet;

    m_team = team;
    app.ui->teamDisplayText.setString(m_team);

    app.ui->hideLeaderboard();
    m_leaderboard.clear();

    const auto& spawns = (m_team == "Terrorist") ? m_tSpawns : m_ctSpawns;
    m_lastSpawnIndex = SIZE_MAX;
    if (!spawns.empty())
    {
        app.camera.setPosition(pickSpawnPoint());
        app.movement_controller.resetVerticalMotion();
    }

    // A previous fall can leave the server's stored position below the map.
    // Send the new spawn over TCP before selecting a team, because selecting
    // a team makes the player eligible for the server's fall-death check.
    const glm::vec3 spawn = app.camera.getPosition();
    const glm::vec3 forward = app.camera.getFront();
    const float yaw = std::atan2(-forward.x, -forward.z);
    app.tcp_client.sendMessage(packet::Room::Player3D::position(
            app.client_api, nexilis::Vector3f({spawn.x, spawn.y, spawn.z}), yaw));

    // The server replies with a stats seed for the local leaderboard.
    app.tcp_client.sendMessage(packet::Room::Player3D::setTeam(app.client_api, team));

    m_health = 100;
    app.ui->healthText.setString("HP: " + std::to_string(m_health));
    app.ui->wastedText.setString("");
    m_prevCamPos = app.camera.getPosition();

    m_weaponPickedUp = true;
    equipWeaponForTeam(app);
}

void DeathmatchMode::update(ClientApp& app, float delta_time)
{
    using packet = nexilis::client::Packet;

    if (m_reloadRemaining > 0.0f)
    {
        m_reloadRemaining = std::max(0.0f, m_reloadRemaining - delta_time);
        if (m_reloadRemaining == 0.0f)
        {
            m_ammo = m_magazineSize;
            refreshWeaponHud(app);
        }
    }

    if (m_hitmarkerFrames > 0)
    {
        m_hitmarkerFrames--;
        app.ui->hitmarkerText.setString(m_hitmarkerFrames > 0 ? "X" : "");
    }
    // Play the hitmarker sound shortly after a hit (handled in the shared
    // GameMode base so it works in every gamemode).
    updateHitmarkerSound(app);

    // --- Gun walk-bob ---
    glm::vec3 cam_pos = app.camera.getPosition();
    glm::vec3 velocity = (cam_pos - m_prevCamPos) / std::max(delta_time, 0.001f);
    m_prevCamPos = cam_pos;

    // Start/stop the footstep loop based on how fast the player is actually
    // moving across the ground (horizontal only, so falling stays quiet).
    updateWalkSound(app, std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z),
                    delta_time);

    // Spatialize sounds other players reported (gunshots and footsteps).
    updateRemoteAudioEvents(app);

    if (m_firstPersonGun)
        m_firstPersonGun->updateAnimation(delta_time, velocity, app.movement_controller.isOnGround());

    // --- Automatic fire while the trigger is held ---
    if (m_health > 0 && m_triggerHeld && m_weaponPickedUp)
    {
        m_fireCooldown -= delta_time;
        // A while loop keeps the cadence correct even after a big frame time.
        while (m_triggerHeld && m_fireCooldown <= 0.0f)
        {
            m_fireCooldown += fireIntervalSeconds();
            fireWeapon(app);
        }
    }

    // --- Consume damage events ---
    auto damageEvents = app.client_api.consumeDamageEvents();
    for (auto& evt : damageEvents)
    {
        if (evt.target_id == app.client_api.getClientId())
        {
            m_health = static_cast<int>(evt.new_health);
            if (m_health < 0)
                m_health = 0;
            app.ui->healthText.setString("HP: " + std::to_string(m_health));
            if (m_health == 0)
            {
                m_triggerHeld = false;
                app.movement_controller.resetVerticalMotion();
            }
            app.camera.addRecoilOffset(m_health == 0 ? -7.0f : -1.5f, 0.0f);
        }
        else if (evt.shooter_id == app.client_api.getClientId() && evt.damage > 0.0f)
        {
            m_hitmarkerFrames = 15;
            triggerHitmarkerSound();
        }
        if (evt.target_id != app.client_api.getClientId())
        {
            if (evt.new_health <= 0.0f)
                app.animatePlayerDeath(evt.target_id);
            else
                app.animatePlayerHit(evt.target_id);
        }
    }

    // --- Consume respawn events ---
    auto respawnEvents = app.client_api.consumeRespawnEvents();
    for (auto& evt : respawnEvents)
    {
        if (evt.target_id == app.client_api.getClientId())
        {
            const auto& spawns = (m_team == "Terrorist") ? m_tSpawns : m_ctSpawns;
            if (!spawns.empty())
            {
                app.camera.setPosition(pickSpawnPoint());
                app.movement_controller.resetVerticalMotion();
            }
            m_prevCamPos = app.camera.getPosition();
            m_triggerHeld = false;
            m_burstShots = 0;
            m_fireCooldown = 0.0f;
            if (m_firstPersonGun)
                m_firstPersonGun->resetAnimation();
            m_health = 100;
            app.ui->healthText.setString("HP: " + std::to_string(m_health));
            app.ui->wastedText.setString("");
            m_ammo = m_magazineSize;
            m_reloadRemaining = 0.0f;
            if (app.reload_audio)
                app.reload_audio->stop();
            refreshWeaponHud(app);

            if (!m_weaponPickedUp)
            {
                m_weaponPickedUp = true;
                equipWeaponForTeam(app);
            }
        }
        else
        {
            app.animatePlayerRespawn(evt.target_id);
        }
    }

    // --- Merge leaderboard stats updates into our local table ---
    auto leaderboardEvents = app.client_api.consumeLeaderboardEvents();
    for (auto& evt : leaderboardEvents)
    {
        for (auto& entry : evt.entries)
        {
            if (entry.id == 0)
                continue;
            m_leaderboard[entry.id] =
                    {entry.id, entry.username, entry.team, entry.kills, entry.deaths};
        }

        // If the Tab board is currently open, refresh it from the local table
        // only (no network round-trip required to show up-to-date numbers).
        if (app.ui->isLeaderboardVisible())
        {
            std::vector<nx3d::client::ui::LeaderboardEntry> uiEntries;
            uiEntries.reserve(m_leaderboard.size());
            for (auto& [id, entry] : m_leaderboard)
                uiEntries.push_back(entry);
            app.ui->showLeaderboard(uiEntries);
        }
    }
}

void DeathmatchMode::handleMouseButtonDown(ClientApp& app, const SDL_Event& event)
{
    if (m_health <= 0 || !m_weaponPickedUp || event.button.button != SDL_BUTTON_LEFT)
        return;
    if (m_reloadRemaining > 0.0f || m_ammo == 0)
        return;

    // Start a fresh burst: the first shot fires immediately, subsequent shots
    // are paced by the weapon's rate of fire while the trigger is held.
    m_triggerHeld = true;
    m_burstShots = 0;
    m_fireCooldown = 0.0f;
    fireWeapon(app);
    m_fireCooldown = fireIntervalSeconds();
}

void DeathmatchMode::handleMouseButtonUp(ClientApp& app, const SDL_Event& event)
{
    if (event.button.button != SDL_BUTTON_LEFT)
        return;

    m_triggerHeld = false;
    m_burstShots = 0;
    m_fireCooldown = 0.0f;
}

float DeathmatchMode::fireIntervalSeconds() const
{
    return 60.0f / getWeaponProfile(m_pickedUpItemType).rounds_per_minute;
}

void DeathmatchMode::fireWeapon(ClientApp& app)
{
    using packet = nexilis::client::Packet;

    if (m_reloadRemaining > 0.0f || m_ammo == 0)
    {
        m_triggerHeld = false;
        return;
    }
    --m_ammo;
    refreshWeaponHud(app);

    // Recoil grows the longer a burst runs: each shot kicks harder than the
    // last, resetting only when the trigger is released.
    m_burstShots++;
    const float recoilMul = std::min(
            kRecoilStartMultiplier + static_cast<float>(m_burstShots) * kRecoilStep,
            kMaxRecoilMultiplier);

    if (app.gunshot_audio)
        app.gunshot_audio->play();

    // Tell every other client where this shot happened so they can play the
    // gunshot sound spatically. Only sent once per trigger: the shoot packet
    // below carries the damage, this one carries the audio origin.
    if (app.ready && app.client_api.clientInRoom())
    {
        glm::vec3 cam_pos = app.camera.getPosition();
        app.tcp_client.sendMessage(packet::Room::Player3D::audioEvent(
                app.client_api, audio::kSoundShoot,
                nexilis::Vector3f({cam_pos.x, cam_pos.y, cam_pos.z})));
    }

    // Capture the aim direction BEFORE the aim punch is applied, so the
    // bullet goes exactly where the crosshair points when firing. The punch
    // below is purely visual feedback and decays after the shot.
    glm::vec3 rayOrigin = app.camera.getPosition();
    glm::vec3 rayDir = glm::normalize(app.camera.getFront());

    if (m_firstPersonGun)
        m_firstPersonGun->addRecoil(0.1f * recoilMul);

    // CS-style aim punch: kick the view up with a little random yaw,
    // decaying over time. Scales up with burst length.
    float yawKick = (static_cast<float>(rand() % 100) - 50.0f) / 50.0f * 0.6f * recoilMul;
    app.camera.addRecoilOffset(1.4f * recoilMul, yawKick);

    auto room_id = app.client_api.clientRoomId();
    auto my_id = app.client_api.getClientId();
    auto players = app.client_api.getRemotePlayersSnapshot(room_id, my_id);

    float closestDist = std::numeric_limits<float>::max();
    uint64_t hitTargetId = 0;

    for (auto& player : players)
    {
        if (app.remote_dead_players.contains(player.id))
            continue;
        const float distance = playerHitDistance(rayOrigin, rayDir,
                                                 {player.x, player.y, player.z}, player.yaw);
        if (distance < closestDist)
        {
            closestDist = distance;
            hitTargetId = player.id;
        }
    }

    if (hitTargetId != 0 && !mapBlocksShot(app, rayOrigin, rayDir, closestDist))
    {
        app.tcp_client.sendMessage(
                packet::Room::Player3D::shoot(app.client_api, hitTargetId, 35.0f));
    }
}

void DeathmatchMode::handleKeyDown(ClientApp& app, const SDL_Event& event)
{
    // Ignore OS auto-repeat: holding Tab/G must only trigger the action once.
    if (event.key.repeat)
        return;

    if (event.key.key == SDLK_R && m_health > 0 && m_weaponPickedUp)
        startReload(app);
    else if (event.key.key == SDLK_G && m_health > 0 && m_weaponPickedUp)
        dropCurrentWeapon(app);
    else if (event.key.key == SDLK_F && m_health > 0 && !m_weaponPickedUp)
        pickupNearbyWeapon(app);
    else if (event.key.key == SDLK_TAB && app.client_api.clientInRoom())
    {
        // Render the locally-maintained leaderboard. No packet is sent: the
        // server keeps clients up to date with kill/team stats as they happen.
        std::vector<nx3d::client::ui::LeaderboardEntry> uiEntries;
        uiEntries.reserve(m_leaderboard.size());
        for (auto& [id, entry] : m_leaderboard)
            uiEntries.push_back(entry);
        app.ui->showLeaderboard(uiEntries);
    }
}

void DeathmatchMode::handleKeyUp(ClientApp& app, const SDL_Event& event)
{
    if (event.key.key == SDLK_TAB)
        app.ui->hideLeaderboard();
}

void DeathmatchMode::equipGun(ClientApp& app,
                              const std::string& weaponName,
                              const std::string& objPath,
                              const std::string& mtlPath,
                              float fpScale,
                              glm::vec3 fpCenter,
                              glm::vec3 fpOffset,
                              glm::vec3 fpRotation)
{
    if (app.reload_audio)
        app.reload_audio->stop();
    if (m_crosshair)
    {
        vkDeviceWaitIdle(app.renderer.getDevice());
        m_crosshair.reset();
        app.ui->renderer().setOverlayCallback(nullptr);
    }
    if (m_firstPersonGun)
    {
        app.scene_manager.getCurrentScene()->removeObject(m_firstPersonGun->getId());
        vkDeviceWaitIdle(app.renderer.getDevice());
        m_firstPersonGun.reset();
    }

    m_firstPersonGun = std::make_shared<nx3d::client::gun::FirstPersonGun>(
            app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
            app.renderer.getCommandPool(), app.renderer.getGraphicsQueue(),
            objPath, mtlPath,
            app.descriptorPool, app.descriptorSetLayout,
            blt::ObjectData(app.shared_world, 0.0f,
                            glm::vec3(0.0f, -1000.0f, 0.0f), app.default_size),
            fpScale, fpCenter);
    m_firstPersonGun->setViewOffset(fpOffset);
    m_firstPersonGun->setViewRotation(fpRotation);
    app.scene_manager.getCurrentScene()->addObject(m_firstPersonGun);

    m_crosshair = std::make_shared<nx3d::client::Crosshair>(
            app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
            app.crosshair_texture, app.descriptorPool, app.descriptorSetLayout,
            blt::ObjectData(app.shared_world, 0.0f,
                            glm::vec3(0.0f, -1000.0f, 0.0f), app.default_size));
    app.ui->renderer().setOverlayCallback([this, &app]()
                                          {
        auto ctx = spear::ui::RenderContext{spear::rendering::vulkan::g_frameContext.commandBuffer};
        if (app.currentState == ClientApp::State::Paused)
            app.ui->renderPauseOverlay(ctx, app.quitHovered);
        else if (app.game_mode && app.game_mode->isEliminated())
            app.ui->renderDeathOverlay(ctx);
        if (app.ui->isLeaderboardVisible())
            app.ui->renderLeaderboardOverlay(ctx);
        app.ui->renderChatOverlay(ctx);
        if (m_crosshair && m_health > 0)
            m_crosshair->render(app.camera); });

    m_weaponName = weaponName;
    const auto& profile = getWeaponProfile(m_pickedUpItemType);
    m_magazineSize = profile.magazine_size;
    m_ammo = m_magazineSize;
    m_reloadRemaining = 0.0f;
    refreshWeaponHud(app);
}

void DeathmatchMode::unequipGun(ClientApp& app)
{
    if (app.reload_audio)
        app.reload_audio->stop();
    if (m_crosshair)
    {
        vkDeviceWaitIdle(app.renderer.getDevice());
        m_crosshair.reset();
        app.ui->renderer().setOverlayCallback(nullptr);
    }
    if (m_firstPersonGun)
    {
        app.scene_manager.getCurrentScene()->removeObject(m_firstPersonGun->getId());
        vkDeviceWaitIdle(app.renderer.getDevice());
        m_firstPersonGun.reset();
    }
    app.ui->weaponHudText.setString("");
    m_weaponName.clear();
    m_reloadRemaining = 0.0f;
}

glm::vec3 DeathmatchMode::pickSpawnPoint()
{
    const auto& spawns = (m_team == "Terrorist") ? m_tSpawns : m_ctSpawns;
    if (spawns.empty())
        return glm::vec3{0.0f};

    size_t index = rand() % spawns.size();
    if (spawns.size() > 1 && index == m_lastSpawnIndex)
        index = (index + 1) % spawns.size();
    m_lastSpawnIndex = index;
    return spawns[index];
}

void DeathmatchMode::equipWeaponForTeam(ClientApp& app)
{
    const bool isCt = (m_team == "Counter Terrorist");
    const auto& profile = getWeaponProfile(isCt ? "m16" : "ak47");

    m_pickedUpItemType = profile.type;

    equipGun(app, profile.name, profile.obj_path, profile.mtl_path,
             profile.fp_scale, profile.fp_center,
             profile.fp_offset, profile.fp_rotation);

    m_pickedUpItemId = 0;
    m_pickedUpItemSize = glm::vec3(1.0f, 1.0f, 1.0f);
}

void DeathmatchMode::refreshWeaponHud(ClientApp& app)
{
    if (m_weaponName.empty())
        return;
    const std::string suffix = m_reloadRemaining > 0.0f ? "  RELOADING" : m_ammo == 0 ? "  [R] RELOAD"
                                                                                      : "";
    app.ui->weaponHudText.setString(m_weaponName + "  " + std::to_string(m_ammo) + "/" +
                                    std::to_string(m_magazineSize) + suffix);
}

void DeathmatchMode::startReload(ClientApp& app)
{
    if (m_reloadRemaining > 0.0f || m_ammo >= m_magazineSize)
        return;
    m_triggerHeld = false;
    m_burstShots = 0;
    m_fireCooldown = 0.0f;
    m_reloadRemaining = getWeaponProfile(m_pickedUpItemType).reload_seconds;
    if (m_firstPersonGun)
        m_firstPersonGun->startReload(m_reloadRemaining);
    refreshWeaponHud(app);
    if (app.reload_audio)
        app.reload_audio->play();
    if (app.ready && app.client_api.clientInRoom())
    {
        const glm::vec3 pos = app.camera.getPosition();
        app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::audioEvent(
                app.client_api, audio::kSoundReload,
                nexilis::Vector3f({pos.x, pos.y, pos.z})));
    }
}

void DeathmatchMode::dropCurrentWeapon(ClientApp& app)
{
    using packet = nexilis::client::Packet;

    if (!app.ready || !app.client_api.clientInRoom())
        return;

    m_weaponPickedUp = false;
    m_triggerHeld = false;
    m_burstShots = 0;
    m_fireCooldown = 0.0f;
    unequipGun(app);

    // If we were holding a map pickup, remove the original from the server.
    if (m_pickedUpItemId != 0)
    {
        app.tcp_client.sendMessage(packet::Room::GameItem::destroy(app.client_api, m_pickedUpItemId));
        m_pickedUpItemId = 0;
    }

    // Place the pickup just ahead of the player. The local physics body gets
    // a forward impulse when the server echoes the newly created item.
    glm::vec3 throwDirection = app.camera.getFront();
    throwDirection.y = 0.0f;
    if (glm::length(throwDirection) < 0.001f)
        throwDirection = glm::vec3(0.0f, 0.0f, -1.0f);
    throwDirection = glm::normalize(throwDirection);
    glm::vec3 dropPos = app.camera.getPosition() + throwDirection * 25.0f;
    dropPos.y -= 10.0f;
    auto pos = nexilis::Vector3f({dropPos.x, dropPos.y, dropPos.z});
    auto dim = nexilis::Vector3f({m_pickedUpItemSize.x, m_pickedUpItemSize.y, m_pickedUpItemSize.z});
    const auto& profile = getWeaponProfile(m_pickedUpItemType);
    app.tcp_client.sendMessage(
            packet::Room::GameItem::create(app.client_api, pos, dim,
                                           profile.type, "on_ground", profile.obj_path));
    app.pending_weapon_throw = ClientApp::PendingWeaponThrow{dropPos, throwDirection};
}

void DeathmatchMode::pickupNearbyWeapon(ClientApp& app)
{
    using packet = nexilis::client::Packet;
    if (!app.ready || !app.client_api.clientInRoom())
        return;

    const glm::vec3 camPos = app.camera.getPosition();
    const auto gameItems = app.client_api.getRemoteGameItemsSnapshot(app.client_api.clientRoomId());
    for (const auto& item : gameItems)
    {
        if (item.status != "on_ground" || !hasWeaponProfile(item.item_type))
            continue;
        glm::vec3 itemPos(item.x, item.y, item.z);
        auto localItem = app.remote_game_items.find(item.id);
        if (localItem != app.remote_game_items.end())
        {
            const btVector3 physicsPos = localItem->second->getPosition();
            itemPos = glm::vec3(physicsPos.x(), physicsPos.y(), physicsPos.z());
        }
        if (glm::distance(camPos, itemPos) >= 100.0f)
            continue;

        m_weaponPickedUp = true;
        m_pickedUpItemId = item.id;
        m_pickedUpItemSize = glm::vec3(item.w, item.h, item.d);
        m_pickedUpItemType = item.item_type;
        app.tcp_client.sendMessage(packet::Room::GameItem::update(app.client_api, item.id, "picked_up"));

        if (localItem != app.remote_game_items.end())
        {
            app.scene_manager.getCurrentScene()->removeObject(localItem->second->getId());
            app.pendingDestroy[(app.frameCount - 1) % 3].push_back(std::move(localItem->second));
            app.remote_game_items.erase(localItem);
        }

        const auto& profile = getWeaponProfile(item.item_type);
        equipGun(app, profile.name, profile.obj_path, profile.mtl_path,
                 profile.fp_scale, profile.fp_center,
                 profile.fp_offset, profile.fp_rotation);
        return;
    }
}

} // namespace nx3d::client::gamemode
