file(GLOB MH_SRCS core/*.cpp
				  mayhem.cpp)

add_executable(${MAYHEM} ${MH_SRCS})
