set(TARGET_NAME EngineDemo_Old)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GAME_ROOT_DIR "${GLAREENGINE_ROOT_DIR}/EngineDemo_Old")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MP")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MP")

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS ${GAME_ROOT_DIR}/*.cpp ${GAME_ROOT_DIR}/*.h)
file(GLOB_RECURSE RESOURCE_FILES CONFIGURE_DEPENDS ${GAME_ROOT_DIR}/*.rc)
file(GLOB_RECURSE DLL_FILES CONFIGURE_DEPENDS ${GLAREENGINE_ROOT_DIR}/packages/*.dll ${ENGINE_ROOT_DIR}/Dependences/*.dll)
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS ${GAME_ROOT_DIR}/Shaders/*.*)

source_group(TREE "${GAME_ROOT_DIR}" FILES ${SOURCE_FILES} ${RESOURCE_FILES} ${SHADER_FILES})

add_executable(${TARGET_NAME} WIN32 ${SOURCE_FILES} ${RESOURCE_FILES})

file(COPY ${DLL_FILES} DESTINATION ${GLAREENGINE_ROOT_DIR}/build/${TARGET_NAME})
file(COPY ${SHADER_FILES} DESTINATION ${GLAREENGINE_ROOT_DIR}/build/${TARGET_NAME}/Shaders)
file(COPY ${SHADER_FILES} DESTINATION ${GLAREENGINE_ROOT_DIR}/bin/${TARGET_NAME}/Shaders)
file(COPY ${RESOURCE_FILES} DESTINATION ${GLAREENGINE_ROOT_DIR}/build/${TARGET_NAME}/Resource)
file(COPY ${DLL_FILES} DESTINATION ${GLAREENGINE_ROOT_DIR}/bin/${TARGET_NAME})   

if(CMAKE_CL_64)
set(LIB_SET ${GLAREENGINE_ROOT_DIR}/packages/directxmesh_desktop_win10.2022.10.18.1/native/lib/x64/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>/DirectXMesh.lib
            ${GLAREENGINE_ROOT_DIR}/packages/directxtex_desktop_win10.2022.10.18.1/native/lib/x64/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>/DirectXTex.lib
            ${GLAREENGINE_ROOT_DIR}/packages/WinPixEventRuntime.1.0.220810001/bin/x64/WinPixEventRuntime.lib
            ${GLAREENGINE_ROOT_DIR}/packages/zlib-msvc-x64.1.2.11.8900/build/native/$<$<CONFIG:Release>:lib_release>$<$<CONFIG:Debug>:lib_debug>/$<$<CONFIG:Release>:zlib>$<$<CONFIG:Debug>:zlibd>.lib
            ${GLAREENGINE_ROOT_DIR}/GlareEngine/Dependences/lib/$<$<CONFIG:Release>:assimp-vc140-mt>$<$<CONFIG:Debug>:assimp-vc142-mtd>.lib)
else(CMAKE_CL_64)
set(LIB_SET ${GLAREENGINE_ROOT_DIR}/packages/directxmesh_desktop_win10.2022.10.18.1/native/lib/x86/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>/DirectXMesh.lib
            ${GLAREENGINE_ROOT_DIR}/packages/directxtex_desktop_win10.2022.10.18.1/native/lib/x86/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>/DirectXTex.lib)
endif(CMAKE_CL_64)

add_custom_command(TARGET ${TARGET_NAME} COMMAND ${CMAKE_COMMAND} -E copy_directory ${GLAREENGINE_ROOT_DIR}/build/${TARGET_NAME}/$<$<CONFIG:Release>:Release>$<$<CONFIG:Debug>:Debug>
                                      ${GLAREENGINE_ROOT_DIR}/bin/${TARGET_NAME})
                                 
target_link_libraries(${TARGET_NAME} ${LIB_SET})

set_target_properties("${TARGET_NAME}" PROPERTIES FOLDER "Game")

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GAME_ROOT_DIR}/RanderEffects>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GAME_ROOT_DIR}/ShaderSource>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GAME_ROOT_DIR}>)

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
