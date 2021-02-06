project(Mayhem)
cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(MAYHEM "mayhem")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_BUILD_TYPE MATCHES Debug)
	message(STATUS "Debug build")
	set(MH_FLAGS "-g -O0 -Wall -std=gnu99")
else()
	message(STATUS "Release build")
	set(MH_FLAGS "-O2 -Wall -std=gnu99")
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/core"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/graphics"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/external"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/external/glad/include")
add_subdirectory(src)
