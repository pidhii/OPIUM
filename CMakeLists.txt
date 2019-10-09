cmake_minimum_required (VERSION 3.15)
project (opium)
enable_language (C)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra -Wno-unused")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -ggdb")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

file (GLOB OPIUM_OBJ_LIB_SRC ${CMAKE_SOURCE_DIR}/src/*.c)
set (OPIUM_INCLUDE ${CMAKE_SOURCE_DIR}/include)

include_directories (${CMAKE_SOURCE_DIR}/include)

add_library (opium_object_lib OBJECT ${OPIUM_OBJ_LIB_SRC})
add_executable (opium ${CMAKE_SOURCE_DIR}/main.c $<TARGET_OBJECTS:opium_object_lib>)

