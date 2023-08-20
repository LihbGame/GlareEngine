set(TARGET_NAME GameDemo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GAME_ROOT_DIR "${GLAREENGINE_ROOT_DIR}/GameDemo")

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS ${GAME_ROOT_DIR}/*.cpp ${GAME_ROOT_DIR}/*.h)
file(GLOB_RECURSE RESOURCE_FILES CONFIGURE_DEPENDS ${GAME_ROOT_DIR}/Resource/*.*)
file(GLOB_RECURSE DLL_FILES CONFIGURE_DEPENDS ${GLAREENGINE_ROOT_DIR}/packages/*.dll ${ENGINE_ROOT_DIR}/Dependences/*.dll)
source_group(TREE "${GAME_ROOT_DIR}" FILES ${SOURCE_FILES} ${RESOURCE_FILES})

add_executable(${TARGET_NAME} WIN32 ${SOURCE_FILES} ${RESOURCE_FILES})


if(CMAKE_CL_64)
set(LIB_SET ${GLAREENGINE_ROOT_DIR}/packages/directxmesh_desktop_win10.2022.10.18.1/native/lib/x64/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>/DirectXMesh.lib
            ${GLAREENGINE_ROOT_DIR}/packages/directxtex_desktop_win10.2022.10.18.1/native/lib/x64/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>/DirectXTex.lib
            ${GLAREENGINE_ROOT_DIR}/packages/WinPixEventRuntime.1.0.220810001/bin/x64/WinPixEventRuntime.lib
            ${GLAREENGINE_ROOT_DIR}/packages/zlib-msvc-x64.1.2.11.8900/build/native/$<$<CONFIG:Release>:lib_release>$<$<CONFIG:Debug>:lib_debug>/$<$<CONFIG:Release>:zlib>$<$<CONFIG:Debug>:zlibd>.lib
            ${GLAREENGINE_ROOT_DIR}/GlareEngine/Dependences/lib/$<$<CONFIG:Release>:assimp-vc140-mt>$<$<CONFIG:Debug>:assimp-vc142-mtd>.lib
            ${GLAREENGINE_ROOT_DIR}/packages/dxc_2023_08_14/lib/x64/dxcompiler.lib)
else(CMAKE_CL_64)
set(LIB_SET ${GLAREENGINE_ROOT_DIR}/packages/directxmesh_desktop_win10.2022.10.18.1/native/lib/x86/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>/DirectXMesh.lib
            ${GLAREENGINE_ROOT_DIR}/packages/directxtex_desktop_win10.2022.10.18.1/native/lib/x86/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>/DirectXTex.lib)
endif(CMAKE_CL_64)

add_custom_command(TARGET ${TARGET_NAME} COMMAND ${CMAKE_COMMAND} -E copy_directory ${GLAREENGINE_ROOT_DIR}/build/${TARGET_NAME}/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>
                                      ${GLAREENGINE_ROOT_DIR}/bin/${TARGET_NAME})

add_custom_command(TARGET ${TARGET_NAME} COMMAND ${CMAKE_COMMAND} -E copy_directory ${GLAREENGINE_ROOT_DIR}/Resource
                                      ${GLAREENGINE_ROOT_DIR}/bin/Resource)
                                   
target_link_libraries(${TARGET_NAME} ${LIB_SET})

set_target_properties("${TARGET_NAME}" PROPERTIES FOLDER "Game")

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GAME_ROOT_DIR}/Source>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GAME_ROOT_DIR}/Resource>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GLAREENGINE_ROOT_DIR}/Resource>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CONFIGURATION_TYPES}>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/Dependences/include/DirectXTK12/Src/Shaders/Compiled>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GLAREENGINE_ROOT_DIR}/packages/directxmesh_desktop_win10.2022.10.18.1/include>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GLAREENGINE_ROOT_DIR}/packages/zlib-msvc-x64.1.2.11.8900/build/native/include>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GLAREENGINE_ROOT_DIR}/packages/WinPixEventRuntime.1.0.220810001/Include/WinPixEventRuntime>)

target_include_directories(
    ${TARGET_NAME}
    PUBLIC $<BUILD_INTERFACE:${GLAREENGINE_ROOT_DIR}/packages/directxtex_desktop_win10.2022.10.18.1/include>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/Dependences/include>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/EngineCore>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/EngineEditor/ImgUIEditor>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/GraphicEffects>)

file(COPY ${DLL_FILES} DESTINATION ${GLAREENGINE_ROOT_DIR}/build/${TARGET_NAME})
file(COPY ${DLL_FILES} DESTINATION ${GLAREENGINE_ROOT_DIR}/bin/${TARGET_NAME})
file(COPY ${RESOURCE_FILES} DESTINATION ${GLAREENGINE_ROOT_DIR}/build/${TARGET_NAME}/Resource)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})

set_property(TARGET ${TARGET_NAME} PROPERTY VS_DPI_AWARE "PerMonitor")
