add_subdirectory(sql)

# The command-line tool drives the test suite; a consumer only wants the library.
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(cli)
endif()
