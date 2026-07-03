cmake_minimum_required(VERSION 3.25)

# --- Auto-detect MSVC environment for Ninja builds --------------------------
# Outside a Developer Command Prompt, Ninja + MSVC can't find system headers
# or libraries. This block detects paths and passes them to CMake properly.
set(_FLUENTUI_NEED_MSVC_ENV OFF)
if(WIN32 AND "$ENV{VSCMD_VER}" STREQUAL "")
    # --- Locate MSVC ---
    file(GLOB _MSVC_VERS
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/*"
    )
    file(GLOB _MSVC_VERS_COMMUNITY
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/*"
    )
    list(APPEND _MSVC_VERS ${_MSVC_VERS_COMMUNITY})
    if(_MSVC_VERS)
        list(SORT _MSVC_VERS ORDER DESCENDING)
        list(GET _MSVC_VERS 0 _MSVC_DIR)
    endif()

    # --- Locate Windows SDK ---
    set(_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
    file(GLOB _SDK_VERS "${_SDK_ROOT}/Lib/10.*")
    if(_SDK_VERS)
        list(SORT _SDK_VERS ORDER DESCENDING)
        list(GET _SDK_VERS 0 _SDK_LIB_DIR)
        get_filename_component(_SDK_VER "${_SDK_LIB_DIR}" NAME)
    endif()

    if(_MSVC_DIR AND _SDK_VER)
        set(_FLUENTUI_NEED_MSVC_ENV ON)

        # Tools (needed before project() for compiler test)
        set(_SDK_BIN "${_SDK_ROOT}/bin/${_SDK_VER}/x64")
        set(CMAKE_RC_COMPILER "${_SDK_BIN}/rc.exe" CACHE FILEPATH "" FORCE)
        set(CMAKE_MT           "${_SDK_BIN}/mt.exe" CACHE FILEPATH "" FORCE)

        # Save paths for use after project() — these become include_directories / link_directories
        set(_FLUENTUI_MSVC_INCLUDE "${_MSVC_DIR}/include")
        set(_FLUENTUI_UCRT_INCLUDE "${_SDK_ROOT}/Include/${_SDK_VER}/ucrt")
        set(_FLUENTUI_UM_INCLUDE   "${_SDK_ROOT}/Include/${_SDK_VER}/um")
        set(_FLUENTUI_SHARED_INCLUDE "${_SDK_ROOT}/Include/${_SDK_VER}/shared")

        set(_FLUENTUI_MSVC_LIB "${_MSVC_DIR}/lib/x64")
        set(_FLUENTUI_UM_LIB   "${_SDK_ROOT}/Lib/${_SDK_VER}/um/x64")
        set(_FLUENTUI_UCRT_LIB "${_SDK_ROOT}/Lib/${_SDK_VER}/ucrt/x64")

        # ENV vars needed only for project() compiler test
        set(ENV{LIB} "${_FLUENTUI_MSVC_LIB};${_FLUENTUI_UM_LIB};${_FLUENTUI_UCRT_LIB}")
        set(ENV{INCLUDE} "${_FLUENTUI_MSVC_INCLUDE};${_FLUENTUI_UCRT_INCLUDE};${_FLUENTUI_UM_INCLUDE};${_FLUENTUI_SHARED_INCLUDE}")
        set(ENV{PATH} "$ENV{PATH};${_SDK_BIN}")

        message(STATUS "Auto-detected MSVC: ${_MSVC_DIR}")
        message(STATUS "Auto-detected Windows SDK: ${_SDK_VER}")
    endif()

    unset(_MSVC_VERS)
    unset(_MSVC_VERS_COMMUNITY)
    unset(_SDK_VERS)
    unset(_SDK_LIB_DIR)
endif()

project(FluentUI VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Apply MSVC system paths for Ninja builds --------------------------------
# These persist into the build step (unlike ENV vars which only affect configure)
if(_FLUENTUI_NEED_MSVC_ENV)
    include_directories(SYSTEM
        "${_FLUENTUI_MSVC_INCLUDE}"
        "${_FLUENTUI_UCRT_INCLUDE}"
        "${_FLUENTUI_UM_INCLUDE}"
        "${_FLUENTUI_SHARED_INCLUDE}"
    )
    link_directories(
        "${_FLUENTUI_MSVC_LIB}"
        "${_FLUENTUI_UM_LIB}"
        "${_FLUENTUI_UCRT_LIB}"
    )
endif()

# --- Options ----------------------------------------------------------------
option(FLUENTUI_BUILD_EXAMPLES "Build FluentUI example application" ON)
option(FLUENTUI_BUILD_TESTS "Build FluentUI unit tests" OFF)

# --- Platform / backend selection (brief 26-B) ------------------------------
# Each profile compiles and links only what it uses:
#   * Enterprise app:  PLATFORM_SDL=ON, BACKEND_GL=ON, BACKEND_VULKAN=OFF
#                      -> builds without the Vulkan SDK.
#   * Embedded engine: PLATFORM_SDL=OFF, BACKEND_VULKAN=ON, BACKEND_GL=OFF
#                      -> the host owns window/surface/events; no SDL/GL symbols.
option(FLUENTUI_PLATFORM_SDL   "Build the SDL3 platform (owned windows + event loop)" ON)
option(FLUENTUI_BACKEND_GL     "Build the OpenGL render backend"  ON)
option(FLUENTUI_BACKEND_VULKAN "Build the Vulkan render backend"  ON)

# Guard: at least one render backend must be enabled.
if(NOT FLUENTUI_BACKEND_GL AND NOT FLUENTUI_BACKEND_VULKAN)
    message(FATAL_ERROR "FluentUI: enable at least one of FLUENTUI_BACKEND_GL / FLUENTUI_BACKEND_VULKAN.")
endif()
# Guard: the GL backend creates/binds its context through SDL (SDL_GL_*), including
# the external-context path, so GL currently requires the SDL platform. The embedded
# (SDL=OFF) profile is Vulkan-only, which supports host-provided external devices.
if(FLUENTUI_BACKEND_GL AND NOT FLUENTUI_PLATFORM_SDL)
    message(FATAL_ERROR "FluentUI: FLUENTUI_BACKEND_GL requires FLUENTUI_PLATFORM_SDL (GL uses SDL for its context). Use the Vulkan backend for the embedded/SDL-free profile.")
endif()
# With PLATFORM_SDL=OFF the host must provide window/surface/events and drive the
# core via CreateStandaloneContext + a PlatformBackend (e.g. NullPlatform). The
# owned driver (FluentApp) and SDLPlatform are not built in that configuration.
if(NOT FLUENTUI_PLATFORM_SDL)
    message(STATUS "FluentUI: PLATFORM_SDL=OFF -> embedded-only (no FluentApp/SDLPlatform; host drives the core).")
endif()

# --- vcpkg detection (only if building standalone) --------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        if(WIN32)
            set(VCPKG_TARGET_TRIPLET "x64-windows")
        elseif(APPLE)
            set(VCPKG_TARGET_TRIPLET "x64-osx")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-linux")
        endif()
    endif()

    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/share")
    elseif(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND NOT DEFINED CMAKE_PREFIX_PATH)
        set(CMAKE_PREFIX_PATH "C:/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/share")
    endif()
endif()

# --- Dependencies -----------------------------------------------------------
# Conditional per platform/backend flags (brief 26-B): each perfil pulls only its
# own dependencies. ZLIB and SDL3_image removed (brief 26-A): ZLIB is a transitive
# dependency of Freetype (its CONFIG package declares it); SDL3_image was linked
# but never used (image loading is done by the vendored stb_image).
find_package(Freetype CONFIG REQUIRED)   # always (font system)
if(FLUENTUI_BACKEND_GL)
    find_package(OpenGL REQUIRED)
endif()
if(FLUENTUI_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)
endif()
if(FLUENTUI_PLATFORM_SDL)
    find_package(SDL3 CONFIG REQUIRED)
endif()

# --- GLAD (OpenGL loader) --------------------------------------------------
# Only built with the GL backend; its headers are needed by OpenGLBackend.h.
if(FLUENTUI_BACKEND_GL)
    add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/glad.c)
    target_include_directories(glad PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    set_target_properties(glad PROPERTIES LINKER_LANGUAGE C POSITION_INDEPENDENT_CODE ON)
endif()

# --- FluentUI library -------------------------------------------------------
add_subdirectory(src)

target_include_directories(FluentUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# --- Link dependencies (brief 26-A/B) ---------------------------------------
# Rule: PUBLIC only what an installed public header exposes; everything else
# PRIVATE so consumers of FluentUI::FluentUI don't inherit it transitively.
target_link_libraries(FluentUI PUBLIC Freetype::Freetype)   # FontSystem.h/Renderer.h expose FT_Library/FT_Face
if(FLUENTUI_BACKEND_GL)
    target_link_libraries(FluentUI PUBLIC  glad)            # OpenGLBackend.h #includes <glad/glad.h>
    target_link_libraries(FluentUI PRIVATE OpenGL::GL)      # loaded via glad; not named in public headers
endif()
if(FLUENTUI_BACKEND_VULKAN)
    target_link_libraries(FluentUI PUBLIC Vulkan::Vulkan)   # VulkanBackend.h exposes Vk types to embedders
endif()
if(FLUENTUI_PLATFORM_SDL)
    target_link_libraries(FluentUI PRIVATE SDL3::SDL3)      # only in internal SDLPlatform.h/.cpp
endif()

# Feature defines — the ONE place backends/platform are branched on. The factories
# (CreateBackendInstance / CreateDefaultPlatform) key off these; the rest of the
# code stays flag-free. PUBLIC so consumers can #ifdef on the same symbols.
target_compile_definitions(FluentUI PUBLIC
    $<$<BOOL:${FLUENTUI_BACKEND_GL}>:FLUENTUI_HAS_GL>
    $<$<BOOL:${FLUENTUI_BACKEND_VULKAN}>:FLUENTUI_HAS_VULKAN>
    $<$<BOOL:${FLUENTUI_PLATFORM_SDL}>:FLUENTUI_HAS_SDL>
)

add_library(FluentUI::FluentUI ALIAS FluentUI)

set(FLUENTUI_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH "FluentUI runtime assets directory" FORCE)

# --- Example application (optional) ----------------------------------------
# The gallery / EngineEditor examples are SDL hosts: they call SDL directly and
# drive FluentApp, so they need the SDL platform. The embedded profile's
# reference host is examples/EmbeddedHost (brief 27).
if(FLUENTUI_BUILD_EXAMPLES AND FLUENTUI_PLATFORM_SDL)
    add_executable(FluentUIExample examples/main.cpp examples/App.cpp examples/EngineEditor.cpp)
    target_include_directories(FluentUIExample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(FluentUIExample PRIVATE FluentUI SDL3::SDL3)  # SDL3 is PRIVATE on FluentUI now
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

# --- Install targets (Phase 6) --------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library (+ glad only when the GL backend is built)
set(_fluentui_install_targets FluentUI)
if(FLUENTUI_BACKEND_GL)
    list(APPEND _fluentui_install_targets glad)
endif()
install(TARGETS ${_fluentui_install_targets}
    EXPORT FluentUITargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers (brief 26-C): explicit exclusion of internals so the installed
# tree only exposes the stable public surface. Backend headers are re-added below
# conditionally, so a consumer never sees a header for a backend that wasn't built.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "SDLPlatform.h"           EXCLUDE   # internal: the only SDL header
    PATTERN "EmbeddedShaders.h"       EXCLUDE   # internal: GL shader blobs
    PATTERN "EmbeddedShadersVulkan.h" EXCLUDE   # internal: Vk shader blobs
    PATTERN "Demo.h"                  EXCLUDE   # internal: demo scaffolding
    PATTERN "stb_image.h"             EXCLUDE   # vendored third-party
    PATTERN "DX11Backend.h"           EXCLUDE   # skeleton without impl (brief 28 relocates)
    PATTERN "OpenGLBackend.h"         EXCLUDE   # re-added below iff BACKEND_GL
    PATTERN "VulkanBackend.h"         EXCLUDE   # re-added below iff BACKEND_VULKAN
)
if(FLUENTUI_BACKEND_GL)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/core/OpenGLBackend.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/core)
    # GLAD headers (needed by OpenGLBackend.h)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
endif()
if(FLUENTUI_BACKEND_VULKAN)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/core/VulkanBackend.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/core)
endif()

# Install assets
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/fluentui/assets
)

# Export targets
install(EXPORT FluentUITargets
    FILE FluentUITargets.cmake
    NAMESPACE FluentUI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FluentUI
)

# Generate config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FluentUIConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FluentUIConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FluentUI
)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FluentUIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FluentUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FluentUIConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FluentUI
)

# --- pkg-config file -------------------------------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fluentui.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/fluentui.pc"
    @ONLY
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fluentui.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# --- Tests (optional) ------------------------------------------------------
if(FLUENTUI_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)
    add_subdirectory(tests)
endif()
