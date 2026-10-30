include(FetchContent)

# To get a release tag
macro(_fetch_git_project NAME REPO COMMIT)
  message(STATUS "Module: ${NAME} @ ${COMMIT}")
  FetchContent_Declare(${NAME}
    GIT_REPOSITORY ${REPO}
    GIT_TAG        ${COMMIT}
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED FALSE
  )
  FetchContent_MakeAvailable(${NAME})
endmacro()

# To get specific commits
macro(_fetch_git_project_hash NAME REPO COMMIT)
  message(STATUS "Module: ${NAME} @ ${COMMIT}")
  FetchContent_Declare(${NAME}
    GIT_REPOSITORY ${REPO}
    GIT_TAG        ${COMMIT}
    UPDATE_DISCONNECTED FALSE
  )
  FetchContent_MakeAvailable(${NAME})
endmacro()

# Add Vulkan (From Khronos' Vulkan Tutorial)
find_package(Vulkan REQUIRED)

add_library(VulkanCppModule INTERFACE)
add_library(Vulkan::cppm ALIAS VulkanCppModule)
target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
target_compile_definitions(VulkanCppModule
  INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

# Define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS to treat VK_ERROR_OUT_OF_DATE_KHR as a success code
target_compile_definitions(${PROJECT_NAME} PRIVATE "VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS")

target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    Vulkan::Vulkan
)

# Add VMA
_fetch_git_project(vma https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git v3.4.0)
target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)

# Add GLM
_fetch_git_project(glm https://github.com/g-truc/glm.git 1.0.3)
target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm)

# Add GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

_fetch_git_project(GLFW https://github.com/glfw/glfw.git 3.4)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# Add STB Image
_fetch_git_project_hash(STBImage https://github.com/nothings/stb.git 28d546d)
target_include_directories(${PROJECT_NAME} PRIVATE ${stbimage_SOURCE_DIR})

# Add Dear ImGui
_fetch_git_project(imgui https://github.com/ocornut/imgui.git v1.92.9b)
target_include_directories(${PROJECT_NAME} PUBLIC ${imgui_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR}/backends)

add_library(ImGui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(ImGui PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(ImGui PRIVATE Vulkan::Vulkan glfw)

target_link_libraries(${PROJECT_NAME} PUBLIC ImGui)