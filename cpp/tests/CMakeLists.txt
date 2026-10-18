add_executable(chess_tests
    test_main.cpp
    test_perft.cpp
    test_scenarios.cpp
    test_pgn.cpp
)
target_link_libraries(chess_tests PRIVATE chess_core)

# Shared test data used by every implementation in this repo.
set(CHESS_TESTDATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/testdata")

add_test(NAME basic     COMMAND chess_tests basic)
add_test(NAME perft     COMMAND chess_tests perft     "${CHESS_TESTDATA_DIR}")
add_test(NAME scenarios COMMAND chess_tests scenarios "${CHESS_TESTDATA_DIR}")
add_test(NAME pgn       COMMAND chess_tests pgn       "${CHESS_TESTDATA_DIR}")
