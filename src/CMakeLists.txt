file(GLOB MH_CORE core/*.cpp
				  graphics/*.cpp)

file(GLOB MH_SRCS mayhem.cpp)

add_library(corelib OBJECT ${MH_CORE})

add_executable(${MAYHEM} ${MH_SRCS} $<TARGET_OBJECTS:corelib>)
target_link_libraries(${MAYHEM} glfw3;GLEW;dl;pthread;X11;GL)
