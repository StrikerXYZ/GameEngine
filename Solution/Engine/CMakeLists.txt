# CMakeList.txt : CMake project for GameEngine, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_library (Engine SHARED "Engine.cpp" "Engine.h" "Core.h" "Minimal.h" "EntryPoint.h" "Definition.h" )

if (WIN32)
    target_compile_definitions(Engine PUBLIC   
       ENGINE_PLATFORM_WINDOWS
       ENGINE_BUILD_DLL
    )   
endif() 

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Engine PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(Engine
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
  )

# TODO: Add tests and install targets if needed.
