add_library(libsql
    ast.cpp
    driver.cpp
    lexer.cpp
    parser.cpp
    print.cpp
    loc.cpp
    sym.cpp
    tok.cpp
)

set_target_properties(libsql PROPERTIES PREFIX "")

target_include_directories(libsql
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(libsql
    PUBLIC
        absl::flat_hash_map
        absl::flat_hash_set
        absl::node_hash_map
        absl::node_hash_set
)

target_compile_features(libsql PUBLIC cxx_std_${CMAKE_CXX_STANDARD}) # make property public.

if (MSVC AND BUILD_SHARED_LIBS)
    target_compile_definitions(libsql PUBLIC ABSL_CONSUME_DLL)
endif()
