# FoxLang runtime library DLLs — one DLL per library
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
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        OUTPUT_NAME "fox.${DLL_NAME}"
        PREFIX ""
        POSITION_INDEPENDENT_CODE ON
    )
endmacro()

# ---- fox.math.dll: sin, cos, tan ----
fox_dll(math
    "${FOX_ROOT}/libs/system/math/SystemFunctionsMath.cpp"
    "${FOX_ROOT}/libs/system/math"
    fox_math.cpp
)

# ---- fox.random.dll ----
fox_dll(random
    "${FOX_ROOT}/libs/system/random/SystemFunctionsRandom.cpp"
    "${FOX_ROOT}/libs/system/random"
    fox_random.cpp
)

# ---- fox.file.dll: file I/O ----
fox_dll(file
    "${FOX_ROOT}/libs/system/fs/SystemFunctionsFile.cpp"
    "${FOX_ROOT}/libs/system/fs"
    fox_file.cpp
)

# ---- fox.util.dll: length, type conversions ----
fox_dll(util
    "${FOX_ROOT}/libs/system/io/util/SystemFunctionUtil.cpp"
    "${FOX_ROOT}/libs/system/io/util"
    fox_util.cpp
)

# ---- fox.socket.dll ----
add_library(socket SHARED
    fox_socket.cpp
    ${FOX_ROOT}/libs/system/api/socket/SystemFunctionsSocket.cpp
)
target_include_directories(socket PRIVATE
    ${COMMON_INCLUDES}
    ${FOX_ROOT}/libs/system/api/socket
)
target_link_libraries(socket PRIVATE fox_rt ws2_32)
set_target_properties(socket PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_NAME "fox.socket"
    PREFIX ""
    POSITION_INDEPENDENT_CODE ON
)

# ---- fox.graphics.dll: GL/GLFW drawing (conditional) ----
if(EXISTS ${FOX_ROOT}/native/glad/include/glad/glad.cpp AND
   EXISTS ${FOX_ROOT}/native/glfw/lib/libglfw3.a)
    add_library(graphics SHARED
        fox_graphics.cpp
        ${FOX_ROOT}/libs/graphics/SystemFunctionsGraphics.cpp
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
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        OUTPUT_NAME "fox.graphics"
        PREFIX ""
        POSITION_INDEPENDENT_CODE ON
    )
endif()
