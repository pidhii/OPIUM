cmake_minimum_required (VERSION 3.15)
project (opium)
enable_language (C)


include (ExternalProject)
find_package (BISON REQUIRED)
find_package (FLEX REQUIRED)


set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra -Wno-unused -Wno-error=cpp -Wno-strict-aliasing -rdynamic")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -ggdb -DOPI_DEBUG_MODE")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Ofast")

set (CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

add_definitions (-Wfatal-errors)

file (GLOB OPIUM_OBJ_LIB_SRC
  ${CMAKE_SOURCE_DIR}/src/*.c
  #${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/codeine/src/*.c)
set (OPIUM_INCLUDE ${CMAKE_SOURCE_DIR}/include)

include_directories (${CMAKE_SOURCE_DIR}/include)
include_directories (${CMAKE_SOURCE_DIR}/codeine/include)


################################################################################
#                                   LibJIT
#
set (LIBJIT_ROOT ${CMAKE_SOURCE_DIR}/libjit)
# configure and build
ExternalProject_Add(libjit_external
  PREFIX ${LIBJIT_ROOT}
  GIT_REPOSITORY "https://git.savannah.gnu.org/git/libjit.git"
  INSTALL_DIR ${LIBJIT_ROOT}/install
  CONFIGURE_COMMAND ${LIBJIT_ROOT}/src/libjit_external/configure --prefix=<INSTALL_DIR>
  BUILD_COMMAND ${MAKE}
  UPDATE_COMMAND ""
  BUILD_BYPRODUCTS <INSTALL_DIR>/lib/libjit.a)
# bootstrap
ExternalProject_Add_Step(libjit_external bootstrap
  COMMAND ./bootstrap
  DEPENDEES download
  DEPENDERS configure
  WORKING_DIRECTORY ${LIBJIT_ROOT}/src/libjit_external)
# wrap into target
add_library (libjit STATIC IMPORTED)
set_target_properties (libjit
  PROPERTIES
    IMPORTED_LOCATION ${LIBJIT_ROOT}/install/lib/libjit.a)
add_dependencies (libjit libjit_external)


################################################################################
#                                  SipHash
#
set (SIPHASH_SRC ${CMAKE_SOURCE_DIR}/siphash/siphash.c)
set (SIPHASH_OUT ${CMAKE_BINARY_DIR}/siphash.c)
set (SIPHASH_PATCH ${CMAKE_SOURCE_DIR}/siphash.diff)
add_custom_target (siphash
  COMMAND patch ${SIPHASH_SRC} -i ${SIPHASH_PATCH} -o ${SIPHASH_OUT}
  BYPRODUCTS ${SIPHASH_OUT}
  SOURCES ${SIPHASH_SRC})


################################################################################
#                                   syntax
#
set (SYNTAX_DIR ${CMAKE_SOURCE_DIR}/src/syntax)

BISON_TARGET (opium_parser ${SYNTAX_DIR}/parser.y ${CMAKE_BINARY_DIR}/parser.c)
FLEX_TARGET (opium_scanner ${SYNTAX_DIR}/lexer.l ${CMAKE_BINARY_DIR}/scanner.c)
ADD_FLEX_BISON_DEPENDENCY (opium_scanner opium_parser)


################################################################################
#                                   opium
#
# objects
add_library (opium_object_lib OBJECT
  ${OPIUM_OBJ_LIB_SRC}
  ${BISON_opium_parser_OUTPUTS}
  ${FLEX_opium_scanner_OUTPUTS}
  ${SIPHASH_OUT})
target_include_directories (opium_object_lib PUBLIC ${LIBJIT_ROOT}/install/include)
add_dependencies (opium_object_lib libjit siphash) # ensure headers are ready
# execuable
add_executable (opium
  ${CMAKE_SOURCE_DIR}/main.c
  $<TARGET_OBJECTS:opium_object_lib>)
target_include_directories (opium PUBLIC ${LIBJIT_ROOT}/install/include)
target_link_libraries (opium libjit -lm -ldl -lpcre -lreadline)
install (
  TARGETS opium
  DESTINATION bin)

set (LIBS_ROOT ${CMAKE_SOURCE_DIR}/libs)

################################################################################
#                                    base
#
# build library
add_library (opium_base_dl SHARED ${LIBS_ROOT}/base/base.c)
set_target_properties (opium_base_dl
  PROPERTIES
    PREFIX ""
    OUTPUT_NAME base)
# install library
install (
  TARGETS opium_base_dl
  DESTINATION lib/opium/baselib)
# install scripts
install (
  FILES ${LIBS_ROOT}/base/base.opi
  DESTINATION lib/opium/baselib)
# install symlink
install (
  FILES ${LIBS_ROOT}/base/base
  DESTINATION lib/opium)

################################################################################
#                                  codeine
#
# install headers
install (
  DIRECTORY
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/codeine/include
  DESTINATION ${CMAKE_INSTALL_PREFIX})

################################################################################
#                                   doctor
#
# install doctor
install (
  PROGRAMS ${CMAKE_SOURCE_DIR}/doctor.opi
  DESTINATION bin
  RENAME doctor)
