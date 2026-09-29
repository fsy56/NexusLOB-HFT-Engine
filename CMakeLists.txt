cmake_minimum_required(VERSION 3.22)
project(NexusLOB CXX)

# Force MSVC to allow extended cache line alignments
add_compile_definitions(_ENABLE_EXTENDED_ALIGNED_STORAGE)

# Enforce strict compliance with C++20 standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define project-wide directory configurations
set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Map all structural source definitions explicitly to guide IntelliSense
set(SOURCE_FILES
    src/main.cpp
)

set(HEADER_FILES
    include/nexus/core/hft_types.hpp
    include/nexus/utils/time_utils.hpp
    include/nexus/utils/csv_compiler.hpp
    include/nexus/core/ring_buffer.hpp
    include/nexus/engine/engine_driver.hpp
)

# Configuration-specific optimization adjustments
if(MSVC)
    # Global warning level for all tracking models
    add_compile_options(/W4 /permissive- /std:c++20)
    
    # Restrict heavy optimizations to Release pipelines to prevent /RTC1 clashes
    add_compile_options("$<$<CONFIG:Release>:/O2;/Oi;/Ot;/GL>")
    add_link_options("$<$<CONFIG:Release>:/LTCG>")
else()
    # High-performance tracking profiles for Clang / GCC optimization models
    add_compile_options(-O3 -march=native -flto -pthread -Wall -Wextra -Wpedantic)
    add_link_options(-flto -pthread)
endif()

# Bind headers and tracking parameters inside executable generation structures
add_executable(nexus_engine 
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# Enforce explicit project-wide search directories for file include declarations
target_include_directories(nexus_engine PRIVATE ${PROJECT_INCLUDE_DIR})

# Set runtime properties to ensure our build directory mimics the root structure
set_target_properties(nexus_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)