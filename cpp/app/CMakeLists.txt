include(FetchContent)

# SDL2 is downloaded and built statically on first configure (needs git + internet once).
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2)

add_executable(chess_app WIN32
    src/main.cpp
    src/App.cpp
    src/Renderer.cpp
    src/PieceArt.cpp
    src/Font.cpp
    src/Theme.cpp
    src/Platform.cpp
    src/ReviewSession.cpp
    src/ReviewUi.cpp
)
set_target_properties(chess_app PROPERTIES OUTPUT_NAME chess)
target_link_libraries(chess_app PRIVATE chess_core chess_review SDL2::SDL2main SDL2::SDL2-static)

# The opening database used by the game review, next to the program.
add_custom_command(TARGET chess_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:chess_app>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/data/openings.tsv"
            "$<TARGET_FILE_DIR:chess_app>/data/openings.tsv")
if(WIN32)
    target_sources(chess_app PRIVATE res/chess.rc)  # embeds the app icon
    target_link_libraries(chess_app PRIVATE comdlg32)
endif()
if(MSVC)
    target_compile_options(chess_app PRIVATE /W4)
    target_compile_definitions(chess_app PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(chess_app PRIVATE -Wall -Wextra)
endif()
