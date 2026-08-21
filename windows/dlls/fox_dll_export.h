#pragma once

// Each FoxVast runtime library DLL exports this function.
// fox.exe calls it at startup to register the library's functions.
//
// Usage in DLL source:
//   #define FOX_DLL_EXPORTS
//   #include "fox_dll_export.h"
//   extern "C" FOX_DLL_API void register_fox_library(LibraryManager* mgr) { ... }

#ifdef _WIN32
#  ifdef FOX_DLL_EXPORTS
#    define FOX_DLL_API __declspec(dllexport)
#  else
#    define FOX_DLL_API __declspec(dllimport)
#  endif
#else
#  define FOX_DLL_API
#endif

class LibraryManager;

extern "C" {
    typedef void (*FoxLibRegisterFunc)(LibraryManager*);
}
