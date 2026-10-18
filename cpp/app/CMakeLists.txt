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
)
set_target_properties(chess_app PROPERTIES OUTPUT_NAME chess)
target_link_libraries(chess_app PRIVATE chess_core SDL2::SDL2main SDL2::SDL2-static)
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
