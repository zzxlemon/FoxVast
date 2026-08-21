// fox.file.dll
#define FOX_DLL_EXPORTS
#define FOX_LIB_FILE
#include "fox_dll_export.h"
#include "library_registry.hpp"

extern "C" FOX_DLL_API void register_fox_library(LibraryManager* mgr) {
    register_file(*mgr);
}
