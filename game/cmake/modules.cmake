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

# Add FastNoise2
set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "Build Noise Tool" FORCE)
_fetch_git_project(FastNoise https://github.com/Auburn/FastNoise2.git v1.1.1)
target_link_libraries(${PROJECT_NAME} PRIVATE FastNoise)
target_include_directories(${PROJECT_NAME} PRIVATE ${fastnoise_SOURCE_DIR})

# Add Physical CPU Count
set(fortran FALSE)
_fetch_git_project(pcc https://github.com/scivision/physical-cpu-count.git v1.0.3)
target_include_directories(${PROJECT_NAME} PRIVATE ${pcc_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} PRIVATE cpu_count)