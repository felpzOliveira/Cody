project(Cody)
cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CODY "cody")
set(SERVER "server")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(CODY_BUILD "Build the editor" ON)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "Debug build")
    set(MH_FLAGS "-g3 -O0 -Wall -std=c++17 -DDEBUG_BUILD -DMEMORY_DEBUG -rdynamic -fshort-enums")
elseif(CMAKE_BUILD_TYPE MATCHES Valgrind)
    message(STATUS "Valgrind build")
    set(MH_FLAGS "-g3 -O0 -Wall -std=c++17 -rdynamic -fshort-enums")
elseif(CMAKE_BUILD_TYPE MATCHES Profiling)
    message(STATUS "Profiling build")
    set(MH_FLAGS "-g -pg -Wall -std=c++17 -fshort-enums")
elseif(CMAKE_BUILD_TYPE MATCHES Memory)
    message(STATUS "Memory check build")
    set(MH_FLAGS "-Ofast -Wall -std=c++17 -DMEMORY_DEBUG -fshort-enums")
else()
    message(STATUS "Release build")
    set(MH_FLAGS "-Ofast -Wall -std=c++17 -fshort-enums")
endif()

if(CODY_BUILD)
    #find_library(PCRE_LIB NAMES pcre libpcre)
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

add_executable(${SERVER} ${SV_SRCS})
add_definitions(${MH_FLAGS})

if(CODY_BUILD)
    file(GLOB MH_SRCS src/cody.cpp)

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external)

    add_library(corelib OBJECT ${MH_CORE})
    add_executable(${CODY} ${MH_SRCS} $<TARGET_OBJECTS:corelib>)

    add_custom_command(TARGET ${CODY} PRE_BUILD COMMAND ${PACK_RESOURCES}
                            ${CMAKE_SOURCE_DIR}
                            ${CMAKE_SOURCE_DIR}/src/resources
                            ${CMAKE_BINARY_DIR}
                        DEPENDS ${MH_SHADERS})

    set (CODY_LIBRARIES
            dl
            ${X11_LIB}
            ${PTHREAD_LIB}
            ${LIB_GIT2_LIBRARIES}
            ${Z_LIB})
     #       ${PCRE_LIB})

    target_link_directories(${CODY} PRIVATE
                            ${CMAKE_CURRENT_BINARY_DIR}/external/libgit2
                            ${CMAKE_CURRENT_BINARY_DIR}/external/libmigdb)
    target_link_libraries(${CODY} PRIVATE ${CODY_LIBRARIES})
endif()

