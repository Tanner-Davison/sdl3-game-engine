cmake_minimum_required(VERSION 3.15)
project(sdl-sandbox
        VERSION 1.0
        DESCRIPTION "SDL3 Game Engine Sandbox"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

set(SOURCES
    src/main.cpp
    src/Window.cpp
    src/Rectangle.cpp
    src/UI.cpp
    src/Button.cpp
    src/Image.cpp
    src/Sprite.cpp
    src/Text.cpp
    src/SpriteSheet.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
)
