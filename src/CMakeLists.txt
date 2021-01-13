file(GLOB MH_CORE core/*.cpp
				  graphics/*.cpp
				  external/glad/src/glad.c)

file(GLOB MH_SRCS mayhem.cpp)

add_library(corelib OBJECT ${MH_CORE})

add_executable(${MAYHEM} ${MH_SRCS} $<TARGET_OBJECTS:corelib>)
target_link_libraries(${MAYHEM} glfw3;dl;pthread;X11;GL)
