#ifndef NX3D_SHARED_PLAYER_HIT_HH
#define NX3D_SHARED_PLAYER_HIT_HH

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace nx3d
{

// The figure origin is at eye height, with boots 64 units below it.
inline float playerHitDistance(const glm::vec3& origin, const glm::vec3& direction,
                               const glm::vec3& playerPosition, float yaw)
{
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const glm::vec3 offset = origin - playerPosition;
    const glm::vec3 localOrigin(c * offset.x - s * offset.z, offset.y,
                                s * offset.x + c * offset.z);
    const glm::vec3 localDirection(c * direction.x - s * direction.z, direction.y,
                                   s * direction.x + c * direction.z);
    const glm::vec3 boxMin(-18.0f, -64.0f, -9.0f);
    const glm::vec3 boxMax(18.0f, 12.0f, 9.0f);

    float nearDistance = -std::numeric_limits<float>::infinity();
    float farDistance = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(localDirection[axis]) < 1e-8f)
        {
            if (localOrigin[axis] < boxMin[axis] || localOrigin[axis] > boxMax[axis])
                return std::numeric_limits<float>::infinity();
            continue;
        }
        float first = (boxMin[axis] - localOrigin[axis]) / localDirection[axis];
        float second = (boxMax[axis] - localOrigin[axis]) / localDirection[axis];
        if (first > second)
            std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance)
            return std::numeric_limits<float>::infinity();
    }
    return farDistance >= 0.0f ? std::max(nearDistance, 0.0f)
                               : std::numeric_limits<float>::infinity();
}

} // namespace nx3d

#endif
