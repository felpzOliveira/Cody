project(Cody)
cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CODY "cody")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_BUILD_TYPE MATCHES Debug)
	message(STATUS "Debug build")
	set(MH_FLAGS "-g3 -O0 -Wall -std=c++11 -DDEBUG_BUILD -DMEMORY_DEBUG -rdynamic")
elseif(CMAKE_BUILD_TYPE MATCHES Valgrind)
	message(STATUS "Valgrind build")
	set(MH_FLAGS "-g3 -O0 -Wall -std=c++11 -rdynamic")
else()
	message(STATUS "Release build")
	set(MH_FLAGS "-O2 -Wall -std=c++11")
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/core"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/languages"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/graphics"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/external"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/resources"
					"${CMAKE_CURRENT_SOURCE_DIR}/src/external/glad/include")
add_subdirectory(src)
