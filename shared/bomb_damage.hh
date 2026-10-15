#ifndef NX3D_SHARED_BOMB_DAMAGE_HH
#define NX3D_SHARED_BOMB_DAMAGE_HH

#include <algorithm>
#include <cmath>

namespace nx3d
{
inline constexpr float kBombBlastRadius = 800.0f;
inline constexpr float kBombMaxDamage = 200.0f;

inline float bombBlastDamage(float distance)
{
    if (!std::isfinite(distance) || distance >= kBombBlastRadius)
        return 0.0f;
    return kBombMaxDamage * (1.0f - std::max(0.0f, distance) / kBombBlastRadius);
}
} // namespace nx3d

#endif
