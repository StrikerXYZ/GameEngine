# Engine Library

file(GLOB_RECURSE SRC_FILES Source/*.cpp)
add_library (Engine STATIC ${SRC_FILES})

target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_LIST_DIR})

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

#target_compile_options(Engine PUBLIC -fsanitize=address)
#target_compile_options(Engine PUBLIC -fsanitize=fuzzer -Zi)
#target_compile_options(Engine PUBLIC -INCREMENTAL:NO -opt:noref -MT)