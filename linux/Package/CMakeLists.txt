set(FOX_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../FoxCore)

# ---- curl (external, for package download) ----
# Windows: bundled curl. Linux: system libcurl (sudo apt install libcurl4-openssl-dev).
if(WIN32)
    set(CURL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/native/curl)
    find_library(CURL_IMPLIB NAMES libcurl.dll.a PATHS ${CURL_ROOT}/lib NO_DEFAULT_PATH)
endif()

# ---- Version (single source of truth, overridable with -DFOXPKG_VERSION=...) ----
set(FOXPKG_VERSION_DEFAULT "0.1.0")

execute_process(
    COMMAND git describe --tags --abbrev=0 --match "foxpkg-v*"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE FOXPKG_GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
set(FOXPKG_VERSION_AUTO "${FOXPKG_VERSION_DEFAULT}")
if(FOXPKG_GIT_TAG)
    string(REGEX REPLACE "^foxpkg-v" "" FOXPKG_VERSION_AUTO "${FOXPKG_GIT_TAG}")
endif()

if(NOT DEFINED FOXPKG_VERSION OR FOXPKG_VERSION STREQUAL "${FOXPKG_VERSION_DEFAULT}")
    set(FOXPKG_VERSION "${FOXPKG_VERSION_AUTO}")
endif()
set(FOXPKG_VERSION "${FOXPKG_VERSION}" CACHE STRING "FoxVast package manager version" FORCE)

add_executable(foxpkg
    package_main.cpp
    tools.cpp
    install/install.cpp
    uninstall/uninstall.cpp
)

target_include_directories(foxpkg PRIVATE
    ${FOX_ROOT}/src
    ${FOX_ROOT}/src/frontend
    ${FOX_ROOT}/src/interpreter
    ${FOX_ROOT}/src/util
    ${FOX_ROOT}/src/vm
)

# Override version via compile definition only when it differs from the
# default baked into version.hpp (keeps VS Code happy with plain .hpp).
if(NOT FOXPKG_VERSION STREQUAL "${FOXPKG_VERSION_DEFAULT}")
    target_compile_definitions(foxpkg PRIVATE "FOXPKG_VERSION=\"${FOXPKG_VERSION}\"")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(foxpkg PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

target_link_libraries(foxpkg PRIVATE fox_rt)

if(WIN32)
    target_include_directories(foxpkg PRIVATE ${CURL_ROOT}/include)
    target_link_libraries(foxpkg PRIVATE ${CURL_IMPLIB} ws2_32 crypt32)
    # Ship libcurl-x64.dll + ca bundle next to foxpkg.exe
    add_custom_command(TARGET foxpkg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CURL_ROOT}/bin/libcurl-x64.dll
            $<TARGET_FILE_DIR:foxpkg>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CURL_ROOT}/bin/curl-ca-bundle.crt
            $<TARGET_FILE_DIR:foxpkg>
    )
else()
    find_package(CURL REQUIRED)
    target_link_libraries(foxpkg PRIVATE CURL::libcurl)
endif()