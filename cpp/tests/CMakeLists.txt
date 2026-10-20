add_executable(chess_tests
    test_main.cpp
    test_perft.cpp
    test_scenarios.cpp
    test_pgn.cpp
    test_review.cpp
)
target_link_libraries(chess_tests PRIVATE chess_core chess_review)

# Shared test data used by every implementation in this repo.
set(CHESS_TESTDATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/testdata")

add_test(NAME basic     COMMAND chess_tests basic)
add_test(NAME perft     COMMAND chess_tests perft     "${CHESS_TESTDATA_DIR}")
add_test(NAME scenarios COMMAND chess_tests scenarios "${CHESS_TESTDATA_DIR}")
add_test(NAME pgn       COMMAND chess_tests pgn       "${CHESS_TESTDATA_DIR}")
add_test(NAME review    COMMAND chess_tests review    "${CHESS_TESTDATA_DIR}")
add_test(NAME import    COMMAND chess_tests import    "${CHESS_TESTDATA_DIR}")
add_test(NAME openings  COMMAND chess_tests openings  "${CHESS_TESTDATA_DIR}")
# Talks to the real Stockfish when engine/stockfish.exe is installed (takes about a minute); otherwise it only runs the parts that need no engine.
add_test(NAME engine    COMMAND chess_tests engine    "${CHESS_TESTDATA_DIR}")
set_tests_properties(engine PROPERTIES TIMEOUT 600)
