add_library(chess_core STATIC
    src/Board.cpp
    src/MoveGen.cpp
    src/Game.cpp
    src/Pgn.cpp
)
target_include_directories(chess_core PUBLIC include)
target_compile_features(chess_core PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(chess_core PRIVATE /W4)
else()
    target_compile_options(chess_core PRIVATE -Wall -Wextra)
endif()
