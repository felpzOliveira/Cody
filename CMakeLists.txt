cmake_policy(SET CMP0048 NEW)

project(Cody VERSION 1.0.0 LANGUAGES CXX C)

cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CODY "cody")
set(SERVER "server")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(BUILD_TYPE "Release" CACHE STRING "Defines the type of build to perform ( default is release )")

option(CODY_BUILD "Build the editor" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${BUILD_TYPE} STREQUAL "Debug")
    message(STATUS "Debug build")
    set(MH_FLAGS "-g3 -O0 -Wall -DDEBUG_BUILD -DMEMORY_DEBUG -rdynamic -fshort-enums")
elseif(${BUILD_TYPE} STREQUAL "Valgrind")
    message(STATUS "Valgrind build")
    set(MH_FLAGS "-g3 -O0 -Wall -rdynamic -fshort-enums")
elseif(${BUILD_TYPE} STREQUAL "Profiling")
    message(STATUS "Profiling build")
    set(MH_FLAGS "-g -pg -Wall -fshort-enums")
elseif(${BUILD_TYPE} STREQUAL "Memory")
    message(STATUS "Memory check build")
    set(MH_FLAGS "-Ofast -Wall -DMEMORY_DEBUG -fshort-enums")
elseif(${BUILD_TYPE} STREQUAL "Release")
    message(STATUS "Release build")
    set(MH_FLAGS "-Ofast -Wall -fshort-enums")
endif()

if(CODY_BUILD)
    find_library(PCRE_LIB NAMES pcre libpcre)
    find_library(X11_LIB NAMES X11)
    find_library(Z_LIB NAMES z)
    find_library(PTHREAD_LIB NAMES pthread)

    set(PACK_RESOURCES ${CMAKE_BINARY_DIR}/pack_resources)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 ${CMAKE_SOURCE_DIR}/cmake/pack_resources.cpp -o pack_resources)
    execute_process(COMMAND ${PACK_RESOURCES} ${CMAKE_SOURCE_DIR}
                        ${CMAKE_SOURCE_DIR}/src/resources
                        ${CMAKE_BINARY_DIR}
                RESULT_VARIABLE PACK_RETURN_CODE
                OUTPUT_VARIABLE PACK_CHK_FILES)

    if(NOT ${PACK_RETURN_CODE} EQUAL 0)
        message(SEND_ERROR ${PACK_CHK_FILES})
    endif()
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/files"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/debugger"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/languages"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/external"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/crypto"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/git"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/glad/include"
                    "${CMAKE_CURRENT_SOURCE_DIR}/external/libgit2/include")

file(GLOB MH_CORE src/core/*.cpp
                  src/files/*.cpp
                  src/resources/*.cpp
                  src/debugger/*.cpp
                  src/external/migdb/*.cpp
                  src/languages/*.cpp
                  src/crypto/*.cpp
                  src/git/*.cpp
                  src/rpc/*.cpp
                  src/graphics/*.cpp
                  src/external/glad/src/*.cpp
                  ${CMAKE_BINARY_DIR}/*.cpp)

file(GLOB SV_SRCS src/files/storage.cpp
                  src/core/memory.cpp
                  src/core/utilities.cpp
                  src/crypto/*.cpp
                  src/rpc/*.cpp
                  src/rpc_server.cpp)

file(GLOB MH_SHADERS shaders/*.glsl)
file(GLOB MH_LANGS lang_tables/*)

if(CODY_BUILD)
    file(GLOB MH_SRCS src/cody.cpp)

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external)

    add_library(corelib OBJECT ${MH_CORE})
    add_executable(${CODY} ${MH_SRCS} $<TARGET_OBJECTS:corelib>)

    add_custom_command(TARGET ${CODY} PRE_BUILD COMMAND ${PACK_RESOURCES}
                            ${CMAKE_SOURCE_DIR}
                            ${CMAKE_SOURCE_DIR}/src/resources
                            ${CMAKE_BINARY_DIR}
                        DEPENDS ${MH_SHADERS} ${MH_LANGS})

    set (CODY_LIBRARIES
            dl
            pthread
            ${X11_LIB}
            ${PTHREAD_LIB}
            ${LIB_GIT2_LIBRARIES}
            ${Z_LIB}
            ${PCRE_LIB})

    target_link_directories(${CODY} PRIVATE
                            ${CMAKE_CURRENT_BINARY_DIR}/external/libgit2
                            ${CMAKE_CURRENT_BINARY_DIR}/external/libmigdb)
    target_link_libraries(${CODY} PRIVATE ${CODY_LIBRARIES})
endif()

add_executable(${SERVER} ${SV_SRCS})
add_definitions(${MH_FLAGS})
target_link_libraries(${SERVER} PRIVATE pthread)

