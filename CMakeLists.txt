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
    set(MH_FLAGS "-Ofast -Wall -std=c++11")
endif()

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

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/files"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/languages"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/external"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/glad/include")

file(GLOB MH_CORE src/core/*.cpp
                  src/files/*.cpp
                  src/resources/*.cpp
                  ${CMAKE_BINARY_DIR}/*.cpp
                  src/languages/*.cpp
                  src/graphics/*.cpp
                  src/external/glad/src/*.cpp)

file(GLOB MH_SHADERS shaders/*.glsl)

file(GLOB MH_SRCS src/cody.cpp)

add_definitions(${MH_FLAGS})
add_library(corelib OBJECT ${MH_CORE})
add_executable(${CODY} ${MH_SRCS} $<TARGET_OBJECTS:corelib>)

add_custom_command(TARGET ${CODY} PRE_BUILD COMMAND ${PACK_RESOURCES}
                        ${CMAKE_SOURCE_DIR}
                        ${CMAKE_SOURCE_DIR}/src/resources
                        ${CMAKE_BINARY_DIR}
                    DEPENDS ${MH_SHADERS})

target_link_libraries(${CODY} dl;X11;pthread)
