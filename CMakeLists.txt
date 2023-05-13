cmake_minimum_required (VERSION 3.15)
project (GlareEngine VERSION 0.1.0)

include(CMakeDependentOption)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MP")

set(GLAREENGINE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INSTALL_PREFIX "${GLAREENGINE_ROOT_DIR}/bin")
set(BINARY_ROOT_DIR "${CMAKE_INSTALL_PREFIX}/")
set(ENGINE_ROOT_DIR "${GLAREENGINE_ROOT_DIR}/GlareEngine")

add_definitions(-DUNICODE -D_UNICODE -D_DEBUG)

add_subdirectory(GlareEngine)
add_subdirectory(GameDemo)
add_subdirectory(EngineDemo_Old)

target_link_libraries(GameDemo GlareEngine)
target_link_libraries(EngineDemo_Old GlareEngine)
