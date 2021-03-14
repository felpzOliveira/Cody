file(GLOB MH_CORE core/*.cpp
				  resources/*.cpp
				  languages/*.cpp
				  graphics/*.cpp
				  external/glad/src/*.cpp)

file(GLOB MH_SRCS cody.cpp)

add_definitions(${MH_FLAGS})
add_library(corelib OBJECT ${MH_CORE})
add_executable(${CODY} ${MH_SRCS} $<TARGET_OBJECTS:corelib>)
target_link_libraries(${CODY} dl;X11)
