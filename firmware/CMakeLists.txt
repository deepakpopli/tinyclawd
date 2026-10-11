cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tinyclawd)

# the pet engine is plain c++17 (same as the desktop sim). idf builds with
# gnu++2b, where <math.h> also brings std::lerp into the global namespace and
# clashes with lerp() in motion.h
idf_component_get_property(pet_lib pet COMPONENT_LIB)
target_compile_options(${pet_lib} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
