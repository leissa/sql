# Not `sql`: a target name is global, and a consumer may well have one of their own.
add_executable(sql-cli main.cpp)
set_target_properties(sql-cli PROPERTIES OUTPUT_NAME sql)
target_compile_definitions(sql-cli PRIVATE SQL_VERSION="${PROJECT_VERSION}")
target_link_libraries(sql-cli libsql)
