cmake_minimum_required(VERSION 3.12)

############ build libgit2
set (BUILD_SHARED_LIBS OFF CACHE BOOL " " FORCE)
set (USE_HTTPS OFF CACHE BOOL " " FORCE)
set (BUILD_TESTS OFF CACHE BOOL " " FORCE)

add_subdirectory(libgit2)
set (LIB_GIT2_INCLUDEDIR ${CMAKE_CURRENT_SOURCE_DIR}/libgit2/include PARENT_SCOPE)
set (LIB_GIT2_LIBRARIES libgit2.a PARENT_SCOPE)
