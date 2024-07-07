# Project Engine + Sandbox

cmake_minimum_required (VERSION 3.26)

project ("Solution" LANGUAGES CXX)

# Enable Hot Reload for MSVC compilers if supported.
#if (POLICY CMP0141)
#  cmake_policy(SET CMP0141 NEW)
#  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
#endif()

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

find_program(CLANG_CL "clang-cl")
set(CMAKE_C_COMPILER "${CLANG_CL}")
set(CMAKE_CXX_COMPILER "${CLANG_CL}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_DEBUG "-Og")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MAP /opt:ref")

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)
#add_definitions(-DASAN_OPTIONS="continue_on_error=1")

add_compile_options(
	-Weverything
	-Wno-c++98-compat
	-Wno-c++98-compat-pedantic
	-Wno-newline-eof
	-Wno-exit-time-destructors
	-Wno-covered-switch-default
	-Wno-missing-prototypes
	-Wno-global-constructors
	-Wno-unused-variable
	-Wno-unused-parameter
	-Wno-extra-semi-stmt
	-Wno-writable-strings
	-Wno-unused-function
	-Wno-unused-template
	-Wno-undef
	-Wno-unused-command-line-argument
	-Wno-unused-value
	-Wno-unsafe-buffer-usage
	-Wno-unused-but-set-variable
	-Wno-ignored-attributes
	-Wno-cast-function-type-strict
	-Wno-cast-function-type		#casey
	-Wno-gnu-anonymous-struct	#casey
	-Wno-nested-anon-types		#casey
	-Wno-missing-braces		#casey
	-WX
	-Oi				#intrinsic optimizations
	-EHs
#	-EHa-			#turn off exceptions
	-MT				#link all windows dependent libraries
)

#find_program(CLANG_TIDY "clang-tidy")
#set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")

# Include sub-projects.
add_subdirectory ("Engine")
add_subdirectory ("Sandbox")
