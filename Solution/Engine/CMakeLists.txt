# CMakeList.txt : CMake project for GameEngine, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_library (Engine STATIC "Minimal.hpp" "Source/Core.hpp" "Source/EntryPoint.hpp" "Source/Definition.hpp" "Source/Log.cpp" "Source/Log.hpp" "Source/EntryPoint.cpp" "Source/Platform/PlatformWindows.cpp")

target_include_directories(Engine PRIVATE
"${CMAKE_CURRENT_LIST_DIR}"
)

add_subdirectory("External/SpdLog")
target_link_libraries(Engine spdlog::spdlog_header_only)

set_target_properties(Engine PROPERTIES
    UNITY_BUILD_MODE BATCH
    UNITY_BUILD_BATCH_SIZE 10
)

if (WIN32)
    target_compile_definitions(Engine PUBLIC   
       ENGINE_PLATFORM_WINDOWS
       ENGINE_BUILD_DLL
    )   
endif() 

target_compile_definitions(Engine PUBLIC

    $<$<CONFIG:Debug>:ENGINE_BUILD_DEBUG=1>
    $<$<CONFIG:Debug>:ENGINE_BUILD_RELEASE=0>

    $<$<CONFIG:Release>:ENGINE_BUILD_DEBUG=0>
    $<$<CONFIG:Release>:ENGINE_BUILD_RELEASE=1>
)




# TODO: Add tests and install targets if needed.
#add_library(spdlog_header_only INTERFACE)
#add_library(spdlog::spdlog_header_only ALIAS spdlog_header_only)

#target_include_directories(
#    spdlog_header_only ${SPDLOG_INCLUDES_LEVEL} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
#                                                          "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
#target_link_libraries(spdlog_header_only INTERFACE Threads::Threads)