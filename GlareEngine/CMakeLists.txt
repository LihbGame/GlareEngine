set(TARGET_NAME GlareEngine)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB ENGINE_RESOURCE CONFIGURE_DEPENDS ${ENGINE_ROOT_DIR}/Resource/GlareEngine.rc)
file(GLOB DEPENDENCES_SOURCES CONFIGURE_DEPENDS ${ENGINE_ROOT_DIR}/Dependences/include/)
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS ${ENGINE_ROOT_DIR}/Shaders/*.hlsl)
file(GLOB_RECURSE SHADER_HEAD CONFIGURE_DEPENDS ${ENGINE_ROOT_DIR}/Shaders/*.hlsli)
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS ${ENGINE_ROOT_DIR}/*.h)
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS ${ENGINE_ROOT_DIR}/*.cpp)
file(GLOB_RECURSE PACKAGE_FILES CONFIGURE_DEPENDS ${GLAREENGINE_ROOT_DIR}/packages/*.h)

source_group(TREE "${ENGINE_ROOT_DIR}" FILES ${ENGINE_RESOURCE} ${DEPENDENCES_SOURCES} ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_SOURCES} ${SHADER_HEAD})
source_group(TREE "${GLAREENGINE_ROOT_DIR}" FILES ${PACKAGE_FILES})

set(shaderPath "${ENGINE_ROOT_DIR}/Shaders")
file(MAKE_DIRECTORY ${shaderPath}/CompiledShaders)

set_source_files_properties(${SHADER_SOURCES} PROPERTIES VS_SHADER_MODEL "6.0")
set_source_files_properties(${SHADER_SOURCES} PROPERTIES VS_SHADER_VARIABLE_NAME "g_p%(Filename)")
set_source_files_properties(${SHADER_SOURCES} PROPERTIES VS_SHADER_OUTPUT_HEADER_FILE "${shaderPath}\\CompiledShaders\\%(Filename).h")
set_source_files_properties(${SHADER_SOURCES} PROPERTIES VS_SHADER_FLAGS "/Zi -Qembed_debug")


file(GLOB_RECURSE VS_SHADER_SOURCES ${ENGINE_ROOT_DIR}/Shaders/*VS.hlsl)
set_source_files_properties(${VS_SHADER_SOURCES} PROPERTIES VS_SHADER_TYPE Vertex)

file(GLOB_RECURSE PS_SHADER_SOURCES ${ENGINE_ROOT_DIR}/Shaders/*PS.hlsl)
set_source_files_properties(${PS_SHADER_SOURCES} PROPERTIES VS_SHADER_TYPE Pixel)

file(GLOB_RECURSE GS_SHADER_SOURCES ${ENGINE_ROOT_DIR}/Shaders/*GS.hlsl)
set_source_files_properties(${GS_SHADER_SOURCES} PROPERTIES VS_SHADER_TYPE Geometry VS_TOOL_OVERRIDE "none")

file(GLOB_RECURSE HS_SHADER_SOURCES ${ENGINE_ROOT_DIR}/Shaders/*HS.hlsl)
set_source_files_properties(${HS_SHADER_SOURCES} PROPERTIES VS_SHADER_TYPE Hull VS_TOOL_OVERRIDE "none")

file(GLOB_RECURSE DS_SHADER_SOURCES ${ENGINE_ROOT_DIR}/Shaders/*DS.hlsl)
set_source_files_properties(${DS_SHADER_SOURCES} PROPERTIES VS_SHADER_TYPE Domain VS_TOOL_OVERRIDE "none")

file(GLOB_RECURSE CS_SHADER_SOURCES ${ENGINE_ROOT_DIR}/Shaders/*CS.hlsl)
set_source_files_properties(${CS_SHADER_SOURCES} PROPERTIES VS_SHADER_TYPE Compute)

add_library(${TARGET_NAME} ${ENGINE_RESOURCE} ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_SOURCES} ${SHADER_HEAD} ${PACKAGE_FILES})

add_custom_command(TARGET ${TARGET_NAME} COMMAND ${CMAKE_COMMAND} -E copy_directory ${GLAREENGINE_ROOT_DIR}/Resource
                                      ${GLAREENGINE_ROOT_DIR}/build/Resource)

set_target_properties("${TARGET_NAME}" PROPERTIES FOLDER "Engine")

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${GLAREENGINE_ROOT_DIR}/Resource>)

target_include_directories(
  ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${shaderPath}>)

  
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


