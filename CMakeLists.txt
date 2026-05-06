cmake_minimum_required(VERSION 3.16)

project(shootah LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(shootah main.c)

target_compile_options(shootah PRIVATE
    $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)

find_package(raylib CONFIG QUIET)

if(raylib_FOUND)
    target_link_libraries(shootah PRIVATE raylib)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RAYLIB REQUIRED IMPORTED_TARGET raylib)
    target_link_libraries(shootah PRIVATE PkgConfig::RAYLIB)
endif()

if(APPLE)
    target_link_libraries(shootah PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()
