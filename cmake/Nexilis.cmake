include_guard(GLOBAL)

# Compile the dependency and its consumers from the same checkout. Importing
# an old .so alongside newer public headers corrupts ClientAPI's event queues.
option(NX3D_USE_SYSTEM_NEXILIS "Use an externally built Nexilis package" OFF)
if(NX3D_USE_SYSTEM_NEXILIS)
    find_package(Nexilis REQUIRED CONFIG)
else()
    set(_nx3d_nexilis_source "${CMAKE_CURRENT_LIST_DIR}/../nexilis/nexilis")
    if(NOT EXISTS "${_nx3d_nexilis_source}/CMakeLists.txt")
        message(FATAL_ERROR "Initialize the Nexilis submodule or set NX3D_USE_SYSTEM_NEXILIS=ON and Nexilis_DIR.")
    endif()
    # Nexilis vendors Boost; with testing enabled the Boost sub-projects try to
    # build their own tests (which need targets that do not exist here).
    set(_nx3d_had_build_testing OFF)
    if(DEFINED BUILD_TESTING)
        set(_nx3d_had_build_testing ON)
        set(_nx3d_saved_build_testing "${BUILD_TESTING}")
    endif()
    set(BUILD_TESTING OFF)
    add_subdirectory("${_nx3d_nexilis_source}" "${CMAKE_BINARY_DIR}/nexilis" EXCLUDE_FROM_ALL)
    if(_nx3d_had_build_testing)
        set(BUILD_TESTING "${_nx3d_saved_build_testing}")
    else()
        unset(BUILD_TESTING)
    endif()
    add_library(Nexilis::nexilis ALIAS nexilis)
endif()
