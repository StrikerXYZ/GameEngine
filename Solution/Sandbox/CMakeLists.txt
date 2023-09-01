# CMakeList.txt : CMake project for GameEngine, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (Game WIN32 "Game.cpp" "Game.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Game PROPERTY CXX_STANDARD 20)
endif()

find_library(ENGINE_LIB Engine)

target_link_libraries(Game user32.lib gdi32.lib Engine)


# TODO: Add tests and install targets if needed.
