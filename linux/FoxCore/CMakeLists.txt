cmake_minimum_required(VERSION 3.16)
project(FoxVast CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---- Shared runtime (value + common) 閿熸枻鎷?linked by fox.exe and every DLL ----
add_library(fox_rt STATIC
    src/frontend/value.cpp
    src/util/common.cpp
)
target_include_directories(fox_rt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interpreter
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm
)
set_target_properties(fox_rt PROPERTIES POSITION_INDEPENDENT_CODE ON)

# value.cpp (Gc) uses pthread_getattr_np on Linux
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(fox_rt PUBLIC Threads::Threads)
endif()

# ---- Main executable ----
# Sources are discovered automatically (GLOB + CONFIGURE_DEPENDS): adding a
# new .cpp under src/ does not require editing this file. CONFIGURE_DEPENDS
# needs CMake >= 3.12; the build re-runs configure when files change.
file(GLOB_RECURSE FOX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# value.cpp / common.cpp are compiled into fox_rt (shared runtime) and are
# linked into fox.exe via fox_rt; exclude them from the executable itself.
list(FILTER FOX_SOURCES EXCLUDE REGEX "/(frontend/value|util/common)\\.cpp$")

add_executable(fox ${FOX_SOURCES})

set_target_properties(fox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/core
)

target_include_directories(fox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interpreter
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm
)

# fox.exe links the shared runtime (value, common)
target_link_libraries(fox PRIVATE fox_rt)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fox PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

if(WIN32)
    target_link_libraries(fox PRIVATE kernel32)
else()
    target_link_libraries(fox PRIVATE ${CMAKE_DL_LIBS})
endif()

# ---- External library DLLs (one per module, placed next to fox.exe) ----
add_subdirectory(../dlls dlls)

# ---- Package manager (separate exe) ----
add_subdirectory(../Package Package)
