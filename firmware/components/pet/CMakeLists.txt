# pet engine: no esp-idf headers in here, the desktop sim builds the same files
file(GLOB PET_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
idf_component_register(SRCS ${PET_SRCS} INCLUDE_DIRS include)
