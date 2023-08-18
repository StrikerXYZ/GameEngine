# CMakeList.txt : CMake project for GameEngine, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (Engine WIN32 "GameEngine.cpp" "GameEngine.h")

target_link_libraries(Engine user32.lib gdi32.lib)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Engine PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
