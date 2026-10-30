cmake_minimum_required(VERSION 3.24)

project(WillowVox LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(WillowVox STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Util.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/App.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/Input.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/jobs/PriorityJobPool.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/VmaUsage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/Window.cpp
)

target_include_directories(WillowVox PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(WillowVox PUBLIC
    $<$<CONFIG:Debug>:DEBUG_MODE>
    $<$<CONFIG:Release>:RELEASE_MODE>
)

target_precompile_headers(WillowVox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/wv/pch.h)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules.cmake OPTIONAL)