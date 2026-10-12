#ifndef NX3D_CLIENT_GAMEMODE_WEAPON_PROFILES_HH
#define NX3D_CLIENT_GAMEMODE_WEAPON_PROFILES_HH

#include <client/project_root.hh>

#include <btBulletDynamicsCommon.h>

#include <glm/glm.hpp>

#include <string>

namespace nx3d::client::gamemode
{

/// Rendering/physics profile of a weapon that can be held in first person or
/// dropped as a physics-driven pickup on the ground.
struct WeaponProfile
{
    std::string name;
    std::string type;
    std::string obj_path;
    std::string mtl_path;
    float ground_scale = 1.0f;
    float fp_scale = 1.0f;
    glm::vec3 fp_center{0.0f};
    glm::vec3 fp_offset{0.68f, -0.58f, 2.25f};
    glm::vec3 fp_rotation{-10.0f, 33.0f, -12.0f};
    btVector3 half_extents{1.0f, 1.0f, 1.0f};
    /// Fully-automatic fire rate in rounds per minute (LMB held down).
    float rounds_per_minute = 600.0f;
    int magazine_size = 30;
    float reload_seconds = 2.5f;
};

/// Whether a weapon profile is known for the given item type.
inline bool hasWeaponProfile(const std::string& type)
{
    return type == "ak47" || type == "m16";
}

/// Get the profile for a weapon type ("ak47" / "m16"). Unknown types fall back
/// to the AK-47 profile, mirroring the original behaviour.
inline const WeaponProfile& getWeaponProfile(const std::string& type)
{
    static const WeaponProfile kAk47 = {
            .name = "AK-47",
            .type = "ak47",
            .obj_path = nx3d::projectAssetPath("weapons/ak47.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/ak47.mtl"),
            .ground_scale = 7.4f,
            .fp_scale = 0.50f,
            .fp_center = glm::vec3(0.0f, -0.05f, 0.20f),
            .fp_offset = glm::vec3(0.68f, -0.58f, 2.25f),
            .fp_rotation = glm::vec3(-10.0f, 33.0f, -12.0f),
            .half_extents = btVector3(1.0f, 4.4f, 11.7f),
            .rounds_per_minute = 600.0f,
            .magazine_size = 30,
            .reload_seconds = 2.5f,
    };

    static const WeaponProfile kM16 = {
            .name = "M16",
            .type = "m16",
            .obj_path = nx3d::projectAssetPath("weapons/m16.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/m16.mtl"),
            .ground_scale = 7.3f,
            .fp_scale = 0.50f,
            .fp_center = glm::vec3(0.0f, -0.05f, 0.30f),
            .fp_offset = glm::vec3(0.70f, -0.59f, 2.30f),
            .fp_rotation = glm::vec3(-9.0f, 35.0f, -11.0f),
            .half_extents = btVector3(1.0f, 4.2f, 11.7f),
            .rounds_per_minute = 800.0f,
            .magazine_size = 30,
            .reload_seconds = 2.5f,
    };

    if (type == "m16")
        return kM16;
    return kAk47;
}

} // namespace nx3d::client::gamemode

#endif
