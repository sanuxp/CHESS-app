# Game review: classification, openings, UCI engine client and analyzer. Needs no graphics.
find_package(Threads REQUIRED)

add_library(chess_review STATIC
    src/Classify.cpp
    src/Material.cpp
    src/Openings.cpp
    src/Review.cpp
    src/Uci.cpp
    src/Analyzer.cpp
)
target_include_directories(chess_review PUBLIC include)
target_link_libraries(chess_review PUBLIC chess_core Threads::Threads)
if(MSVC)
    target_compile_options(chess_review PRIVATE /W4)
    target_compile_definitions(chess_review PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(chess_review PRIVATE -Wall -Wextra)
endif()
