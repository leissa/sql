find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(RUN_TEST ${CMAKE_CURRENT_SOURCE_DIR}/run_test.py)

# Registers one CTest entry per fixture in `dir`, running run_test.py in `mode`.
function(add_sql_tests dir mode)
    file(GLOB fixtures CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.sql)
    foreach(fixture ${fixtures})
        get_filename_component(name ${fixture} NAME_WE)
        add_test(
            NAME ${mode}/${dir}/${name}
            COMMAND ${Python3_EXECUTABLE} ${RUN_TEST} --sql $<TARGET_FILE:sql> --mode ${mode} ${fixture}
        )
    endforeach()
endfunction()

# Curated fixtures: the dump is pinned by a golden file and must round-trip.
add_sql_tests(parse parse)
add_sql_tests(parse idempotent)

# Fixtures that must be rejected, with their diagnostics pinned by a golden file.
add_sql_tests(error error)

# Corpora of one query per line, every one of which must be rejected. No goldens - these only
# assert that nothing slips through.
add_sql_tests(reject reject)

# The Join Order Benchmark plus its schema, and TPC-H: large real-world corpora. No goldens -
# these only assert that every query parses and that its dump round-trips.
add_sql_tests(job idempotent)
add_sql_tests(tpch idempotent)

add_subdirectory(bench)

# Regenerate every golden file: `cmake --build build --target bless`.
add_custom_target(bless
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/bless.py
            --sql $<TARGET_FILE:sql> --test-dir ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS sql
    COMMENT "Regenerating test golden files"
)
