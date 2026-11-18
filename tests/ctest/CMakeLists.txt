# M61: registers the existing checks as one CTest testcase each, so JUnit counts real programs.
# Labels select the kind: unit (what `make test` runs), trace (the [REQ-...] subset), perf, e2e.
# Generated JUnit: `make test-tm-<kind>` -> test-results/<kind>.xml. Run through `make check`.
cmake_minimum_required(VERSION 3.20)
project(lift_pilot_checks NONE)
enable_testing()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
find_program(TM_MAKE make REQUIRED)
find_program(TM_PYTHON NAMES python3.14 python3 REQUIRED)

function(tm_check name labels)
  add_test(NAME "${name}" COMMAND ${ARGN})
  set_tests_properties("${name}" PROPERTIES WORKING_DIRECTORY "${ROOT}" LABELS "${labels}")
endfunction()

tm_check("test_flight [REQ-glide-physics]" "unit;trace" ./test_flight)
tm_check("test_pitch [REQ-pitch-model]" "unit;trace" ./test_pitch)
tm_check("test_gif" "unit" ./test_gif)
tm_check("test_observer [REQ-glide-physics]" "unit;trace" ./test_observer)
tm_check("test_ferry [REQ-ferry]" "unit;trace" ./test_ferry)
tm_check("story_bench --check [REQ-ferry]" "unit;trace" ./story_bench --check docs/narrative)
tm_check("theory-check" "unit" ${TM_PYTHON} scripts/theory_check.py)
tm_check("bench" "perf" ${TM_MAKE} -j1 bench)
tm_check("smoke" "e2e" ${TM_MAKE} -j1 smoke)
