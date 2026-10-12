cmake_minimum_required(VERSION 3.30)
project(nx_3D_client)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fallback package location when NX3D_USE_SYSTEM_NEXILIS is explicitly enabled.
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../nexilis/nexilis/build/")

include(../cmake/Nexilis.cmake)

add_subdirectory(../spear/engine spear_engine)

# nixpkgs' shaderc package ships libshaderc_shared but the pkg-config
# advertises shaderc_combined. Fix the spear target's link libraries.
get_target_property(_spear_links spear INTERFACE_LINK_LIBRARIES)
if(_spear_links)
  string(REPLACE "shaderc_combined" "shaderc_shared" _spear_links "${_spear_links}")
  set_target_properties(spear PROPERTIES INTERFACE_LINK_LIBRARIES "${_spear_links}")
endif()

file(GLOB_RECURSE NX3D_CLIENT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
list(FILTER NX3D_CLIENT_SOURCES EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/build/")

add_executable(nx_3D_client ${NX3D_CLIENT_SOURCES})
if(NX3D_PORTABLE_CLIENT)
  set(PROJECT_ROOT ".")
  # The bundled client runs from its archive directory; Spear assets live in spear/.
  foreach(_property COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(_definitions spear ${_property})
    if(_definitions)
      list(FILTER _definitions EXCLUDE REGEX "^SPEAR_ROOT=")
      list(APPEND _definitions "SPEAR_ROOT=\"./spear\"")
      set_target_properties(spear PROPERTIES ${_property} "${_definitions}")
    endif()
  endforeach()
else()
  set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()
target_include_directories(nx_3D_client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(nx_3D_client PRIVATE PROJECT_ROOT="${PROJECT_ROOT}")
target_link_libraries(nx_3D_client PRIVATE Nexilis::nexilis spear)
if(WIN32)
  target_link_libraries(nx_3D_client PRIVATE ws2_32)
endif()
