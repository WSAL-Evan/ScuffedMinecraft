cmake_minimum_required(VERSION 3.24)

project(ScuffedMinecraft LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ScuffedMinecraft
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/physics/VoxelRaycast.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/world/BuildChunkMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/world/ChunkManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/world/VoxelLighting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/world/WorldGen.cpp
)

target_include_directories(ScuffedMinecraft PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(ScuffedMinecraft PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_link_libraries(ScuffedMinecraft PRIVATE WillowVox)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets)

target_compile_definitions(ScuffedMinecraft PUBLIC
    $<$<CONFIG:Debug>:DEBUG_MODE>
    $<$<CONFIG:Release>:RELEASE_MODE>
)

target_precompile_headers(ScuffedMinecraft REUSE_FROM WillowVox)

# Compile shaders
find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

set(SHADER_TARGET ScuffedMCShaders)
set(SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/assets/shaders)
set(SHADER_SOURCES "${CMAKE_CURRENT_LIST_DIR}/shaders/shader.slang")
set(ENTRY_POINTS -entry vertMain -entry fragMain)
add_custom_command(OUTPUT ${SHADERS_DIR} COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR})
add_custom_command(
    OUTPUT ${SHADERS_DIR}/shader.spv
    COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_DIR}/shader.spv
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
    COMMENT "Compiling Slang Shaders"
    VERBATIM
)
add_custom_target(${SHADER_TARGET} DEPENDS ${SHADERS_DIR}/shader.spv)
add_dependencies(ScuffedMinecraft ${SHADER_TARGET})

# Copy assets folder to build folder
add_custom_command(
  TARGET ScuffedMinecraft POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory 
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:ScuffedMinecraft>/assets
  COMMENT "Copying assets"
)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules.cmake OPTIONAL)