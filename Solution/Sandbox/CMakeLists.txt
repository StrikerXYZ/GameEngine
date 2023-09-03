# CMakeList.txt : CMake project for GameEngine, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (Game WIN32 "Game.cpp" )

target_link_libraries(Game PUBLIC
user32.lib 
gdi32.lib 
Engine
#"${CMAKE_CURRENT_SOURCE_DIR}/../Build/x64-debug/lib/Engine.lib"
)

target_include_directories(Game PUBLIC
"${PROJECT_BINARY_DIR}"
"${PROJECT_SOURCE_DIR}/Engine"
)

#if (CMAKE_VERSION VERSION_GREATER 3.12)
#  set_property(TARGET Game PROPERTY CXX_STANDARD 20)
#endif()




# TODO: Add tests and install targets if needed.
