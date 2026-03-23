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
find_package(OpenGL REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(Freetype CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SDL3_image REQUIRED)

# --- GLAD (OpenGL loader) --------------------------------------------------
add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/glad.c)
target_include_directories(glad PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C POSITION_INDEPENDENT_CODE ON)

# --- FluentUI library -------------------------------------------------------
add_subdirectory(src)

target_include_directories(FluentUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(FluentUI PUBLIC
    OpenGL::GL
    glad
    SDL3::SDL3
    SDL3_image::SDL3_image
    Freetype::Freetype
    ZLIB::ZLIB
)

add_library(FluentUI::FluentUI ALIAS FluentUI)

set(FLUENTUI_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH "FluentUI runtime assets directory" FORCE)

# --- Example application (optional) ----------------------------------------
if(FLUENTUI_BUILD_EXAMPLES)
    add_executable(FluentUIExample examples/main.cpp examples/App.cpp examples/EngineEditor.cpp)
    target_include_directories(FluentUIExample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(FluentUIExample PRIVATE FluentUI)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

# --- Install targets (Phase 6) --------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library
install(TARGETS FluentUI glad
    EXPORT FluentUITargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Install GLAD headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

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
