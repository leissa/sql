cmake_minimum_required(VERSION 3.29 FATAL_ERROR)
project(SQLParser VERSION 0.1.0)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
if(MSVC)
    add_compile_options(/utf-8 /wd4146 /wd4245)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(BUILD_SHARED_LIBS "Build shared libraries." ON)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}; shared libs: ${BUILD_SHARED_LIBS}")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Make `make test`/`ninja test` build first instead of testing a stale binary.
    set(CMAKE_SKIP_TEST_ALL_DEPENDENCY FALSE)
    include(CTest) # defines BUILD_TESTING
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX) # prevents windows.h defining min/max macros
else()
    add_compile_options(-Wall -Wextra)
endif()

# An embedded libsql is built into its consumer, which has a prefix of its own to keep tidy.
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(sql_install ON)
else()
    set(sql_install OFF)
endif()
option(SQL_INSTALL "If ON, generate install rules for libsql." ${sql_install})
# libsql's public headers include fe's, so a consumer needs them - and the export set needs fe in one.
set(FE_INSTALL ${SQL_INSTALL})
add_subdirectory(submodules/fe)
add_subdirectory(src)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(example) # built here so it cannot rot.
endif()

if(BUILD_TESTING AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(test)
endif()
