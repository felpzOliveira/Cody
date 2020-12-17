project(Mayhem)
cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(MAYHEM "mayhem")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_BUILD_TYPE MATCHES Debug)
	message(STATUS "Debug build")
	set(MH_FLAGS "-g -O0")
else()
	message(STATUS "Release build")
	set(MH_FLAGS "-O3")
endif()

add_subdirectory(src)
