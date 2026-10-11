#include <spear/camera.hh>
#include <spear/movement_controller.hh>

#include <btBulletDynamicsCommon.h>

#include <cmath>
#include <iostream>
#include <unordered_map>

int main()
{
    btDefaultCollisionConfiguration config;
    btCollisionDispatcher dispatcher(&config);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &config);
    btStaticPlaneShape floor(btVector3(0.0f, 1.0f, 0.0f), -128.0f);
    btRigidBody::btRigidBodyConstructionInfo floorInfo(0.0f, nullptr, &floor);
    btRigidBody floorBody(floorInfo);
    world.addRigidBody(&floorBody);

    spear::Camera camera(glm::vec3(0.0f, -48.0f, 0.0f));
    spear::MovementController movement(camera, &world, 64.0f);
    movement.resetVerticalMotion();
    const std::unordered_map<SDL_Keycode, bool> keys{
            {SDLK_W, false}, {SDLK_S, false}, {SDLK_A, false},
            {SDLK_D, false}, {SDLK_SPACE, false}};

    // A half-second frame crosses the CT floor entirely. Ground detection
    // must still land on it instead of starting its ray below the surface.
    movement.processInput(keys, 0.5f);
    if (std::abs(camera.getPosition().y - (-64.0f)) > 0.01f || !movement.isOnGround())
    {
        std::cerr << "Player fell through the CT spawn floor\n";
        return 1;
    }
    return 0;
}
