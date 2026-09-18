cmake_minimum_required(VERSION 3.29 FATAL_ERROR)

# Standalone, this needs an installed sql; as part of the main build the alias is already there.
if(NOT TARGET sql::sql)
    project(sql-example CXX)
    find_package(sql REQUIRED)
endif()

add_executable(example example.cpp)
target_link_libraries(example PRIVATE sql::sql)
