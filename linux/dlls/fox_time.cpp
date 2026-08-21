// fox.time.dll
#define FOX_DLL_EXPORTS
#define FOX_LIB_TIME
#include "fox_dll_export.h"
#include "library_registry.hpp"

extern "C" FOX_DLL_API void register_fox_library(LibraryManager* mgr) {
    register_time(*mgr);
}