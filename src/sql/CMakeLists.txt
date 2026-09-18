add_library(libsql
    driver.cpp
    lexer.cpp
    parser.cpp
    sql.cpp
    stream.cpp
    tok.cpp
)
add_library(sql::sql ALIAS libsql)
set_target_properties(libsql PROPERTIES PREFIX "")
set_target_properties(libsql PROPERTIES DEFINE_SYMBOL fe_EXPORTS)
set_target_properties(libsql PROPERTIES EXPORT_NAME sql) # `libsql::libsql` reads badly.
set_target_properties(libsql PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

target_sources(libsql
    PUBLIC
        FILE_SET headers
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        TYPE HEADERS
        FILES
            ${PROJECT_SOURCE_DIR}/include/sql/ast.h
            ${PROJECT_SOURCE_DIR}/include/sql/driver.h
            ${PROJECT_SOURCE_DIR}/include/sql/lexer.h
            ${PROJECT_SOURCE_DIR}/include/sql/parser.h
            ${PROJECT_SOURCE_DIR}/include/sql/sql.h
            ${PROJECT_SOURCE_DIR}/include/sql/tok.h
)
target_link_libraries(libsql
    PUBLIC
        absl::flat_hash_map absl::flat_hash_set
        absl::node_hash_map absl::node_hash_set
        fe
)
target_compile_features(libsql PUBLIC cxx_std_${CMAKE_CXX_STANDARD}) # make property public.

if (MSVC AND BUILD_SHARED_LIBS)
    target_compile_definitions(libsql PUBLIC ABSL_CONSUME_DLL)
endif()

if(SQL_INSTALL)
    set(targets_export_name "sql-targets")

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/sql-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion # pre-1.0, a minor bump may break.
    )
    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/sql-config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/sql-config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/sql"
    )
    install(
        TARGETS libsql
        EXPORT ${targets_export_name}
        FILE_SET headers
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/sql-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/sql-config-version.cmake"
        DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/sql"
    )
    install(
        EXPORT ${targets_export_name}
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/sql"
        NAMESPACE sql::
    )
endif()
