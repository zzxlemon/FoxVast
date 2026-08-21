#pragma once

class LibraryManager;

// Load all fox.*.dll files found next to fox.exe and call their
// register_fox_library() entry points.  Returns true if at least one
// DLL was loaded successfully.
bool LoadFoxLibs(LibraryManager& libMgr);
