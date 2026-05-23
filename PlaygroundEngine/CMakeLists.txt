cmake_minimum_required(VERSION 4.2.3)

add_library(PlaygroundEngine)

target_sources(PlaygroundEngine
    PRIVATE
        World.cpp

    PUBLIC
        FILE_SET HEADERS
        FILES
            World.h
)

