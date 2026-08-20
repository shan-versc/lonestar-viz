cmake_minimum_required(VERSION 3.24)
project(aarf_cartographer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── options ──────────────────────────────────────────────────────────────────
option(AARF_BUILD_TESTS                "Build the unit test suite"      ON)
option(AARF_TREAT_WARNINGS_AS_ERRORS   "First-party warnings -> errors" ON)
option(AARF_FETCHCONTENT_DISCONNECTED  "No network fetch; use cache"    OFF)

# ── compiler warnings ──────────────────────────────────────────────────────────
# Applied to first-party targets only (via aarf_apply_warnings) so that
# FetchContent dependencies are not polluted with -Werror.
function(aarf_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(AARF_TREAT_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(AARF_TREAT_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Allow fully offline / cached builds (see the AARF_FETCHCONTENT_DISCONNECTED option).
set(FETCHCONTENT_FULLY_DISCONNECTED  ${AARF_FETCHCONTENT_DISCONNECTED})
set(FETCHCONTENT_UPDATES_DISCONNECTED ${AARF_FETCHCONTENT_DISCONNECTED})

# ── EnTT ──────────────────────────────────────────────────────────────────────
FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG        v3.13.2
  GIT_SHALLOW    TRUE
)

# ── GLFW ──────────────────────────────────────────────────────────────────────
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
  GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS      OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
# Disable Wayland on Linux CI runners that don't have wayland-scanner
set(GLFW_BUILD_WAYLAND   OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11       ON  CACHE BOOL "" FORCE)

# ── glad2 (OpenGL 4.1 core) ───────────────────────────────────────────────────
# We use the glad2 cmake sub-directory which provides glad_add_library().
# FetchContent downloads the repo; we point SOURCE_SUBDIR at cmake/ so that
# FetchContent_MakeAvailable doesn't try to build the repo root (which has no
# top-level CMakeLists in glad2).
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG        v2.0.8
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  cmake
)

# ── GLM ───────────────────────────────────────────────────────────────────────
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE
)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

# ── Dear ImGui ────────────────────────────────────────────────────────────────
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.90.8
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(entt glfw glad glm nlohmann_json imgui)

# ── Generate glad GL 4.1 core library ─────────────────────────────────────────
# FetchContent_MakeAvailable(glad) runs cmake/CMakeLists.txt which calls
# include(GladConfig.cmake) — so glad_add_library() is now in scope.
glad_add_library(glad_gl41 REPRODUCIBLE API gl:core=4.1)

# ── ImGui as a static lib ─────────────────────────────────────────────────────
add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# ── main executable ───────────────────────────────────────────────────────────
# octree_physics.cpp is owned by this feature branch (octree-accelerated layout)
add_executable(aarf_cartographer
  src/main.cpp
  src/registry.cpp
  src/physics.cpp
  src/octree_physics.cpp
  src/renderer.cpp
  src/ingest.cpp
)

target_include_directories(aarf_cartographer PRIVATE src)

# GLM experimental extensions (gtx/quaternion, gtx/norm, etc.)
target_compile_definitions(aarf_cartographer PRIVATE GLM_ENABLE_EXPERIMENTAL)

# First-party warning flags (incl. -Werror when enabled)
aarf_apply_warnings(aarf_cartographer)

target_link_libraries(aarf_cartographer PRIVATE
  EnTT::EnTT
  glfw
  glad_gl41
  glm::glm
  nlohmann_json::nlohmann_json
  imgui_lib
)

# POSIX threads (Linux/macOS)
find_package(Threads REQUIRED)
target_link_libraries(aarf_cartographer PRIVATE Threads::Threads)

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(aarf_cartographer PRIVATE OpenGL::GL)

# Platform: link dl on Linux (needed by glad2 at runtime)
if(UNIX AND NOT APPLE)
  target_link_libraries(aarf_cartographer PRIVATE dl)
endif()

# ── unit tests ─────────────────────────────────────────────────────────────────
if(AARF_BUILD_TESTS)
  enable_testing()
  add_executable(aarf_tests
    tests/test_core.cpp
    src/registry.cpp
    src/physics.cpp
    src/octree_physics.cpp
  )
  target_include_directories(aarf_tests PRIVATE src)
  target_compile_definitions(aarf_tests PRIVATE GLM_ENABLE_EXPERIMENTAL)
  target_link_libraries(aarf_tests PRIVATE EnTT::EnTT glm::glm)
  aarf_apply_warnings(aarf_tests)
  add_test(NAME aarf_core COMMAND aarf_tests)
endif()
