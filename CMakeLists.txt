# CMakeList.txt : CMake project for FluentGUI, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.25)
project(FluentUI LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows/share")

# Ruta de código fuente
add_subdirectory(src)

# GLAD library
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)



# Para usar SDL3, SDL3_image (opcional) y FreeType desde vcpkg
find_package(SDL3 CONFIG REQUIRED)
find_package(Freetype CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SDL3_image REQUIRED)

# Example executable
add_executable(FluentUIExample examples/main.cpp)
target_include_directories(FluentUIExample PRIVATE ${CMAKE_SOURCE_DIR}/include)

file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

target_link_libraries(FluentUI PUBLIC opengl32 glad SDL3::SDL3 SDL3_image::SDL3_image Freetype::Freetype ZLIB::ZLIB)
target_link_libraries(FluentUIExample PRIVATE FluentUI)
