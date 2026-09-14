# Not a CTest entry - a benchmark is not a pass/fail test. Build it with
# `cmake --build build --target bench` and point it at a corpus:
#     ./build/bin/bench test/job/*.sql
add_executable(bench EXCLUDE_FROM_ALL bench.cpp)
target_link_libraries(bench libsql)
