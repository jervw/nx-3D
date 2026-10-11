#include <client/gamemode/bomb_player_state.hh>

int main()
{
    using nx3d::client::gamemode::shouldShowEliminated;
    if (shouldShowEliminated(0, false, false)) // Just joined.
        return 1;
    if (shouldShowEliminated(0, true, false)) // Admitted, waiting for first round.
        return 1;
    if (shouldShowEliminated(1, false, false)) // Queued spectator.
        return 1;
    if (shouldShowEliminated(1, true, true)) // Spawned.
        return 1;
    if (!shouldShowEliminated(1, true, false)) // Killed during a round.
        return 1;
    return 0;
}
