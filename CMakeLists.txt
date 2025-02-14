# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Part of the Tit Solver project, under the MIT License.
# See /LICENSE.md for license information. SPDX-License-Identifier: MIT
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

cmake_minimum_required(VERSION 3.20..3.27)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Setup the project.
project(tit VERSION 0.1 LANGUAGES CXX)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Setup path to CMake modules and include them.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(clang)
include(codespell)
include(python)
include(tit_target)
include(tit_testing)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Find the third-party libraries.
find_package(
  Boost
  CONFIG REQUIRED
  COMPONENTS core container stacktrace_addr2line
)
find_package(Crow CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)
find_package(gcem CONFIG REQUIRED)
find_package(hwy CONFIG REQUIRED)
find_package(metis CONFIG REQUIRED)
find_package(mimalloc CONFIG REQUIRED)
find_package(
  Python
  REQUIRED
  COMPONENTS
    Interpreter
    Development
    Development.SABIModule
)
find_package(nanobind REQUIRED) # Should be after Python.
find_package(TBB CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Add the actual sources.
add_subdirectory("manual")
add_subdirectory("source")
add_subdirectory("tests")

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Create a meta target we would use to get all the compile flags.
# This target must depend on all the third-party libraries, and therefore has
# all the needed compile flags: include directories, compile definitions, etc.
add_tit_library(
  NAME
    all_flags
  DEPENDS
    Boost::container
    Boost::core
    Boost::stacktrace_addr2line
    Crow::Crow
    doctest::doctest
    gcem
    hwy::hwy
    metis
    mimalloc-static
    nanobind::nanobind
    Python::Python
    TBB::tbb
    TBB::tbbmalloc
    unofficial::sqlite3::sqlite3
    zstd::libzstd
)

# Write "compile_flags.txt" based on the meta library.
write_compile_flags(tit::all_flags)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# Enable spell checking.
check_spelling()

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
