cmake_minimum_required(VERSION 3.16)
project(FoxLang CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---- Shared runtime (value + common) — linked by fox.exe and every DLL ----
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

# ---- Main executable ----
set(SOURCES
    src/main.cpp
    src/frontend/ast.cpp
    src/frontend/lexer.cpp
    src/frontend/parser.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/library_init.cpp
    src/util/utils.cpp
    src/util/dll_loader.cpp
    src/vm/bytecode.cpp
    src/vm/far.cpp
    # System library implementations are linked from libs/;
    # DLL builds export the same functions via dlls/
    libs/graphics/SystemFunctionsGraphics.cpp
    libs/system/api/socket/SystemFunctionsSocket.cpp
    libs/system/fs/SystemFunctionsFile.cpp
    libs/system/io/util/SystemFunctionUtil.cpp
    libs/system/math/SystemFunctionsMath.cpp
    libs/system/random/SystemFunctionsRandom.cpp
    native/glad/include/glad/glad.cpp
)

add_executable(fox ${SOURCES})

target_include_directories(fox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interpreter
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm
    ${CMAKE_CURRENT_SOURCE_DIR}/native
    ${CMAKE_CURRENT_SOURCE_DIR}/native/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/native/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/graphics
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/system/api/socket
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/system/fs
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/system/io/util
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/system/math
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/system/random
)

target_link_directories(fox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/native/glfw/lib)

# fox.exe links the shared runtime (value, common)
target_link_libraries(fox PRIVATE fox_rt)

# Static fallback — fox.exe compiles all libraries
target_compile_definitions(fox PRIVATE
    FOX_LIB_MATH
    FOX_LIB_RANDOM
    FOX_LIB_FILE
    FOX_LIB_UTIL
    FOX_LIB_SOCKET
    FOX_LIB_GRAPHICS
)

if(WIN32)
    target_compile_options(fox PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
    target_link_libraries(fox PRIVATE ws2_32 glfw3 opengl32 gdi32 user32 kernel32)
else()
    target_link_libraries(fox PRIVATE glfw m)
endif()

# ---- External library DLLs (one per module, placed next to fox.exe) ----
add_subdirectory(../dlls dlls)
