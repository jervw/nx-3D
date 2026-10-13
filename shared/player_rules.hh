#ifndef NX3D_SHARED_PLAYER_RULES_HH
#define NX3D_SHARED_PLAYER_RULES_HH

namespace nx3d
{
// de_dust2 spans about X [-2560, 1920], Z [-3776, 1536], and Y [-192, 416].
// Leave room for the player capsule, jumps, and the fall-death plane at -256.
inline constexpr float kWalkSpeed = 280.0f;
// Walking while jumping reaches about 377 units/s in three dimensions.
inline constexpr float kMaxMovementSpeed = 400.0f;
inline constexpr float kMaxHorizontalPosition = 5000.0f;
inline constexpr float kMinPlayerHeight = -512.0f;
inline constexpr float kMaxPlayerHeight = 1024.0f;

// The M16 fires at 800 RPM (13.33 shots/s); both current rifles deal 35 damage.
inline constexpr float kMaxShootingRate = 16.0f;
inline constexpr float kMaxDamagePerShot = 35.0f;
} // namespace nx3d

#endif
