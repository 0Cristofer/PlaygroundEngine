cmake_minimum_required(VERSION 4.2.3)

add_executable(PlaygroundGame)

target_sources(PlaygroundGame
    PRIVATE
        main.cpp
)

target_link_libraries(PlaygroundGame
    PRIVATE
        PlaygroundEngine
)