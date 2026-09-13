cmake_minimum_required(VERSION 3.20)
project(keyfinder-shim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

#
# FFTW3, double precision, static.
#
# libkeyfinder ships cmake/FindFFTW3.cmake, which calls find_library() without
# constraining the suffix and would pick up a shared library. Seeding the same
# cache entries here with a static archive makes that find_library() a no-op.
#
set(FFTW3_ROOT "" CACHE PATH "Root of an FFTW3 installation containing lib/libfftw3.a")

if(NOT FFTW3_ROOT)
  message(FATAL_ERROR
    "FFTW3_ROOT is not set. Build through ./build.sh, which builds FFTW from a "
    "pinned, checksummed tarball and passes the prefix in. A host FFTW is not "
    "used: this binary ships inside a signed .app, so the GPL source offer has "
    "to name the exact version linked into it.")
endif()

set(_saved_find_library_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
find_library(FFTW3_LIBRARY
  NAMES fftw3
  HINTS "${FFTW3_ROOT}/lib"
  DOC "FFTW3 library"
  REQUIRED)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_saved_find_library_suffixes})

find_path(FFTW3_INCLUDE_DIR
  NAMES fftw3.h
  HINTS "${FFTW3_ROOT}/include"
  DOC "FFTW3 include directory"
  REQUIRED)

if(NOT FFTW3_LIBRARY MATCHES "\\.a$")
  message(FATAL_ERROR
    "FFTW3_LIBRARY must be a static archive so the shim has no runtime "
    "dependency on Homebrew; got '${FFTW3_LIBRARY}'. Pass -DFFTW3_ROOT=<prefix> "
    "or -DFFTW3_LIBRARY=<path to libfftw3.a>.")
endif()

#
# libkeyfinder 2.2.8, static. Pinned by commit: a tag is a movable ref and
# upstream can re-point it, which would change what ships without a diff here.
#
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
# libkeyfinder declares cmake_minimum_required(VERSION 3.5), which CMake 4 rejects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

FetchContent_Declare(libkeyfinder
  GIT_REPOSITORY https://github.com/mixxxdj/libkeyfinder.git
  GIT_TAG b33b5a88e04a5182dd19c38c57762925631118fd  # tag 2.2.8
  GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(libkeyfinder)

# libkeyfinder exports its repository root as the include directory, but the
# headers live one level down in src/.
add_executable(keyfinder-shim main.cpp)
target_include_directories(keyfinder-shim PRIVATE "${libkeyfinder_SOURCE_DIR}/src")
target_link_libraries(keyfinder-shim PRIVATE keyfinder)
