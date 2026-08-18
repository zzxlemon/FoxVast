# FoxVast runtime library DLLs — one DLL per library
# Each DLL exports register_fox_library(LibraryManager*).

set(FOX_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../FoxCore)
set(COMMON_INCLUDES
    ${FOX_ROOT}/src
    ${FOX_ROOT}/src/frontend
    ${FOX_ROOT}/src/interpreter
    ${FOX_ROOT}/src/util
)

macro(fox_dll DLL_NAME LIB_CPP LIB_INCLUDE)
    add_library(${DLL_NAME} SHARED ${ARGN})
    target_sources(${DLL_NAME} PRIVATE ${LIB_CPP})
    target_include_directories(${DLL_NAME} PRIVATE ${COMMON_INCLUDES} ${LIB_INCLUDE})
    target_link_libraries(${DLL_NAME} PRIVATE fox_rt)
    set_target_properties(${DLL_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/core
        OUTPUT_NAME "fox.${DLL_NAME}"
        PREFIX ""
        POSITION_INDEPENDENT_CODE ON
    )
endmacro()

# ---- fox.math.dll: sin, cos, tan ----
file(GLOB MATH_SOURCES CONFIGURE_DEPENDS "${FOX_ROOT}/libs/system/math/*.cpp")
fox_dll(math
    "${MATH_SOURCES}"
    "${FOX_ROOT}/libs/system/math"
    fox_math.cpp
)

# ---- fox.random.dll ----
file(GLOB RANDOM_SOURCES CONFIGURE_DEPENDS "${FOX_ROOT}/libs/system/random/*.cpp")
fox_dll(random
    "${RANDOM_SOURCES}"
    "${FOX_ROOT}/libs/system/random"
    fox_random.cpp
)

# ---- fox.file.dll: file I/O ----
file(GLOB FILE_SOURCES CONFIGURE_DEPENDS "${FOX_ROOT}/libs/system/fs/*.cpp")
fox_dll(file
    "${FILE_SOURCES}"
    "${FOX_ROOT}/libs/system/fs"
    fox_file.cpp
)

# ---- fox.util.dll: length, type conversions ----
file(GLOB UTIL_SOURCES CONFIGURE_DEPENDS "${FOX_ROOT}/libs/system/io/util/*.cpp")
fox_dll(util
    "${UTIL_SOURCES}"
    "${FOX_ROOT}/libs/system/io/util"
    fox_util.cpp
)

# ---- fox.time.dll: epoch seconds, strftime formatting ----
file(GLOB TIME_SOURCES CONFIGURE_DEPENDS "${FOX_ROOT}/libs/system/time/*.cpp")
fox_dll(time
    "${TIME_SOURCES}"
    "${FOX_ROOT}/libs/system/time"
    fox_time.cpp
)

# ---- fox.socket.dll ----
file(GLOB SOCKET_SOURCES CONFIGURE_DEPENDS
    "${FOX_ROOT}/libs/system/api/socket/*.cpp")
add_library(socket SHARED
    fox_socket.cpp
    ${SOCKET_SOURCES}
)
target_include_directories(socket PRIVATE
    ${COMMON_INCLUDES}
    ${FOX_ROOT}/libs/system/api/socket
)
target_link_libraries(socket PRIVATE fox_rt ws2_32)
set_target_properties(socket PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/core
    OUTPUT_NAME "fox.socket"
    PREFIX ""
    POSITION_INDEPENDENT_CODE ON
)

# ---- fox.graphics.dll: GL/GLFW drawing (conditional) ----
if(EXISTS ${FOX_ROOT}/native/glad/include/glad/glad.cpp AND
   EXISTS ${FOX_ROOT}/native/glfw/lib/libglfw3.a)
    file(GLOB GRAPHICS_SOURCES CONFIGURE_DEPENDS
        "${FOX_ROOT}/libs/graphics/*.cpp")
    add_library(graphics SHARED
        fox_graphics.cpp
        ${GRAPHICS_SOURCES}
        ${FOX_ROOT}/native/glad/include/glad/glad.cpp
    )
    target_include_directories(graphics PRIVATE
        ${COMMON_INCLUDES}
        ${FOX_ROOT}/libs/graphics
        ${FOX_ROOT}/native
        ${FOX_ROOT}/native/glad/include
        ${FOX_ROOT}/native/glfw/include
    )
    target_link_directories(graphics PRIVATE ${FOX_ROOT}/native/glfw/lib)
    target_link_libraries(graphics PRIVATE fox_rt glfw3 opengl32 gdi32 user32 kernel32)
    set_target_properties(graphics PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/core
        OUTPUT_NAME "fox.graphics"
        PREFIX ""
        POSITION_INDEPENDENT_CODE ON
    )
endif()
