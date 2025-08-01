cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
cmake_policy(SET CMP0048 NEW)

project(Cody VERSION 1.0.0 LANGUAGES CXX C)

# For windows use: cmake .. -G"Visual Studio 17 2022"
#                  MSBuild .\Cody.sln /p:Configuration=Release

set(CODY "cody")
set(SERVER "server")
set(LEX_TOOL "ltool")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(CODY_BUILD "Build the editor" ON)

# for windows we need C++-20 so MSCV can actually build our structs definition
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build")
    add_definitions(-DDEBUG_BUILD -DDMEMORY_DEBUG)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/files
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/debugger
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/languages
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/external
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/external/glad/include)

# core directory
set(CORE_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/app.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/memory.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/control_cmds.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/viewer_ctrl.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/keyboard.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/tries.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scroll_controller.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/undo.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/query_bar.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/base_cmd.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/autocomplete.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/selectable.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/transform.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/display.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/parallel.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/theme.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/view.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/pdfview.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/bufferview.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/buffers.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/modal.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/lex.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utilities.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/symbol.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/encoding.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/hash.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/buffers.cpp)

# graphics directory
list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/graphics_querybar.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/graphics.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/image_renderer.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/graphics_image_render.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/graphics_ctrl_cmds_render.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/graphics_default.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/graphics_selector.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/popupwindow.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/dbgwidget.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/graphics_buffer_view.cpp)

# audio directory
list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/mp3decoder.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audioworker.cpp)

# files directory
list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/files/file_buffer.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/files/file_base_hooks.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/files/file_provider.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/files/storage.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/files/view_tree.cpp)

# crypto directory
list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/cryptoutil.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/secure_hash.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/rng.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/aes.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/blake2.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/argon2.cpp)

# debugger directory
list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debugger/dbg.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/debugger/dbgapp.cpp)
# glad
list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/external/glad/src/glad.cpp)

# rpc directory
list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/net_services.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/security_services.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/rpc_services.cpp)

# resources pre-defined
list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/image_util.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/extra_fonts.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/const_font.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/languages/none.cpp)

# resources that need to be generated
set(GEN_FILES "${CMAKE_CURRENT_BINARY_DIR}/src/languages/cmake.cpp"
              "${CMAKE_CURRENT_BINARY_DIR}/src/languages/glsl.cpp"
              "${CMAKE_CURRENT_BINARY_DIR}/src/languages/cpp.cpp"
              "${CMAKE_CURRENT_BINARY_DIR}/src/languages/tex.cpp"
              "${CMAKE_CURRENT_BINARY_DIR}/src/languages/lit.cpp"
              "${CMAKE_CURRENT_BINARY_DIR}/src/languages/py.cpp"
              "${CMAKE_CURRENT_BINARY_DIR}/src/resources/const_shader.cpp"
              "${CMAKE_CURRENT_BINARY_DIR}/src/resources/const_icons.cpp")

set(SERVER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/files/storage.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/core/memory.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utilities.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/core/encoding.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/cryptoutil.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/secure_hash.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/blake2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/argon2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/rng.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/aes.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/net_services.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/security_services.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/rpc_services.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc_server.cpp)

file(GLOB MH_SHADERS shaders/*.glsl)
file(GLOB MH_LANGS lang_tables/*)

# create empty files for cmake
foreach(FILE ${GEN_FILES})
    if(NOT EXISTS ${FILE})
        file(WRITE ${FILE} "")
    endif()
endforeach()

list(APPEND CORE_FILES ${GEN_FILES})

if(CODY_BUILD)
    if(CMAKE_SYSTEM_NAME MATCHES "Windows")
        set(PACK_RESOURCES ${CMAKE_BINARY_DIR}/pack_resources.exe)
        # TODO: This is working, however it would be best if we could specify ${CMAKE_CXX_COMPILER}
        # also these flags are only ok for cl. It seems that using ${CMAKE_CXX_COMPILER} instead of cl
        # when compiling with visual studio generates an error. I have no idea why, but it does...
        add_custom_target(CompileResources
                COMMAND ${CMAKE_COMMAND} -E echo "Compiling resources packer..."
                COMMAND cl /EHsc /std:c++17 ${CMAKE_SOURCE_DIR}/cmake/pack_resources.cpp
                DEPENDS ${CMAKE_SOURCE_DIR}/cmake/pack_resources.cpp)
    else()
        set(PACK_RESOURCES ${CMAKE_BINARY_DIR}/pack_resources)
        add_custom_target(CompileResources
                COMMAND ${CMAKE_COMMAND} -E echo "Compiling resources packer..."
                COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 ${CMAKE_SOURCE_DIR}/cmake/pack_resources.cpp -o pack_resources
                DEPENDS ${CMAKE_SOURCE_DIR}/cmake/pack_resources.cpp)
    endif()
    add_custom_target(ResourcesRunner
                COMMAND ${CMAKE_COMMAND} -E echo "Running resources packer..."
                COMMAND ${PACK_RESOURCES} ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/src/resources ${CMAKE_CURRENT_BINARY_DIR}/src
                DEPENDS CompileResources ${MH_SHADERS} ${MH_LANGS})

    if(NOT ${PACK_RETURN_CODE} EQUAL 0)
        message(SEND_ERROR ${PACK_CHK_FILES})
    endif()
endif()


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/core/windows
                        ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/windows
                        ${LIB_GIT2_INCLUDEDIR}
                        ${LIB_SDL3_INCLUDEDIR})
    list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/core/windows/win_keyboard.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/core/windows/timer.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/windows/win_display.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/windows/wgl_helper.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/debugger/dbg_windows.cpp)
    # on windows make sure we have /DEBUG:FULL
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set (LINK_OPTS "/DEBUG:FULL")
    endif()
    # prevent subsystem to be console
    set(SUBSYSTEM WIN32)
else()
    find_library(PTHREAD_LIB NAMES pthread)
    if(CODY_BUILD)
        find_library(X11_LIB NAMES X11)
    endif()

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/core/linux
                        ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/linux
                        ${LIB_GIT2_INCLUDEDIR}
                        ${LIB_SDL3_INCLUDEDIR})

    list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/core/linux/x11_keyboard.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/linux/x11_display.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/linux/glx_helper.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/debugger/dbg_linux.cpp)

    # migdb directory
    list(APPEND CORE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/thread.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/var_obj.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/breakpoint.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/cpp_int.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/prg_control.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/stack_man.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/symbol_query.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/misc.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/parse.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/data_man.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/connect.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/alloc.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/target_man.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/error.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/get_free_vt.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/external/migdb/get_free_pty.cpp)

    if(CODY_BUILD)
        set (CODY_LIBRARIES
                dl
                ${X11_LIB}
                ${PTHREAD_LIB}
                ${LIB_GIT2_TARGET}
                ${LIB_SDL3_TARGET})
    endif()
endif()

if(CODY_BUILD)
    ############################################################################
    find_library(POPPLER_CPP_LIB NAMES poppler-cpp)
    if(POPPLER_CPP_LIB)
        add_definitions(-DPOPPLER_CPP_ENABLED)
        list(APPEND CODY_LIBRARIES ${POPPLER_CPP_LIB})
        message(STATUS "Enabling Poppler interface")
    else()
        message(STATUS "Poppler not found, disabling PDF view")
    endif()
    ############################################################################
    set(EDITOR_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/cody.cpp)

    add_library(corelib OBJECT ${CORE_FILES})
    add_dependencies(corelib ResourcesRunner)
    add_executable(${CODY} ${SUBSYSTEM} ${EDITOR_FILES}
                   $<TARGET_OBJECTS:corelib>)

    #add_custom_command(TARGET ${CODY} PRE_BUILD COMMAND ${PACK_RESOURCES}
     #                       ${CMAKE_SOURCE_DIR}
      #                      ${CMAKE_CURRENT_BINARY_DIR}/src/resources
       #                     ${CMAKE_CURRENT_BINARY_DIR}/src
        #                DEPENDS ${MH_SHADERS} ${MH_LANGS})

    target_link_libraries(${CODY} PRIVATE ${CODY_LIBRARIES})
    target_link_options(${CODY} PRIVATE ${LINK_OPTS})

    install(TARGETS ${CODY} RUNTIME DESTINATION /bin)
    #install(FILES ${DESKTOP_FILE} DESTINATION ~/.local/share/applications)
    #install(FILES ${ICON_FILE} DESTINATION ~/.local/share/icons/hicolor/128x128/apps)
endif()

set(LEX_TOOL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ltool.cpp)
add_executable(${LEX_TOOL} ${SUBSYSTEM} ${LEX_TOOL_SOURCES} $<TARGET_OBJECTS:corelib>)
target_link_libraries(${LEX_TOOL} PRIVATE ${CODY_LIBRARIES})
target_link_options(${LEX_TOOL} PRIVATE ${LINK_OPTS})

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    # TODO: Prevent compilation of the server in windows for now
    message(STATUS "Skipping server for windows (not implemented)")
else()
    add_executable(${SERVER} ${SERVER_FILES})
    target_link_libraries(${SERVER} PRIVATE ${PTHREAD_LIB})
endif()

