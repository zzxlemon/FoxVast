// Static fallback — used when library DLLs are not found at run time.
// All libraries are compiled directly into fox.exe.
// FOX_LIB_* macros are set via CMake (target_compile_definitions).
#include "library_registry.hpp"

void initSystemLibraries() {
    auto& mgr = LibraryManager::getInstance();
    register_all_libraries(mgr);
}