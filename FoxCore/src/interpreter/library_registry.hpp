// FoxLang runtime library registration - single source of truth.
// Each library section is guarded by a FOX_LIB_<NAME> macro so that
// each DLL compiles only its own registration.
//
// To add a new function:
//   1. Implement it in libs/system/<lib>/SystemFunctions<Lib>.cpp
//   2. Add registration call in the appropriate section below
//   3. Done - fox.exe loads the function from fox.<lib>.dll.

#pragma once
#include "library_manager.hpp"
#include <vector>

// ============================================================
//  fox.std.math  — sin, cos, tan
// ============================================================
#ifdef FOX_LIB_MATH
#include "../../libs/system/math/SystemFunctionsMath.h"
inline void register_math(LibraryManager& mgr) {
    mgr.registerLibrary("math");
    mgr.registerLibraryName("math", "fox.std.math");
    mgr.registerSystemFunction("math", "sin", [](const std::vector<Value>& args) -> Value {
        Math math;
        return math.sin(args);
    });
    mgr.registerSystemFunction("math", "cos", [](const std::vector<Value>& args) -> Value {
        Math math;
        return math.cos(args);
    });
    mgr.registerSystemFunction("math", "tan", [](const std::vector<Value>& args) -> Value {
        Math math;
        return math.tan(args);
    });
}
#endif

// ============================================================
//  fox.std.random
// ============================================================
#ifdef FOX_LIB_RANDOM
#include "../../libs/system/random/SystemFunctionsRandom.h"
inline void register_random(LibraryManager& mgr) {
    mgr.registerLibrary("random");
    mgr.registerLibraryName("random", "fox.std.random");
    mgr.registerSystemFunction("random", "random", [](const std::vector<Value>& args) -> Value {
        Random random;
        return random.RandomStart(args);
    });
}
#endif

// ============================================================
//  fox.sys.io.fs  — file I/O
// ============================================================
#ifdef FOX_LIB_FILE
#include "../../libs/system/fs/SystemFunctionsFile.h"
inline void register_file(LibraryManager& mgr) {
    mgr.registerLibrary("file");
    mgr.registerLibraryName("file", "fox.sys.io.fs");
    mgr.registerSystemFunction("file", "file_open", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileOpen(args);
    });
    mgr.registerSystemFunction("file", "file_read", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileRead(args);
    });
    mgr.registerSystemFunction("file", "file_write", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileWrite(args);
    });
    mgr.registerSystemFunction("file", "file_close", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileClose(args);
    });
    mgr.registerSystemFunction("file", "file_remove", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileDelete(args);
    });
}
#endif

// ============================================================
//  fox.std.util  — length, type conversions
// ============================================================
#ifdef FOX_LIB_UTIL
#include "../../libs/system/io/util/SystemFunctionUtil.h"
inline void register_util(LibraryManager& mgr) {
    mgr.registerLibrary("util");
    mgr.registerLibraryName("util", "fox.std.util");
    mgr.registerSystemFunction("util", "length", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.length(args);
    });
    mgr.registerSystemFunction("util", "IntChangeString", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.IntChangeString(args);
    });
    mgr.registerSystemFunction("util", "StringChangeInt", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.StringChangeInt(args);
    });
    mgr.registerSystemFunction("util", "StringChangeDouble", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.StringChangeDouble(args);
    });
    mgr.registerSystemFunction("util", "DoubleChangeString", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.DoubleChangeString(args);
    });
    mgr.registerSystemFunction("util", "DoubleChangeInt", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.DoubleChangeInt(args);
    });
    mgr.registerSystemFunction("util", "IntChangeDouble", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.IntChangeDouble(args);
    });
    mgr.registerSystemFunction("util", "arr_append", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.arr_append(args);
    });
    mgr.registerSystemFunction("util", "arr_pop", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.arr_pop(args);
    });
    mgr.registerSystemFunction("util", "arr_contains", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.arr_contains(args);
    });
    mgr.registerSystemFunction("util", "arr_slice", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.arr_slice(args);
    });
    mgr.registerSystemFunction("util", "arr_sort", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.arr_sort(args);
    });
    mgr.registerSystemFunction("util", "arr_length", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.arr_length(args);
    });
    mgr.registerSystemFunction("util", "str_contains", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.str_contains(args);
    });
    mgr.registerSystemFunction("util", "str_replace", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.str_replace(args);
    });
    mgr.registerSystemFunction("util", "str_split", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.str_split(args);
    });
    mgr.registerSystemFunction("util", "str_trim", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.str_trim(args);
    });
    mgr.registerSystemFunction("util", "str_lower", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.str_lower(args);
    });
    mgr.registerSystemFunction("util", "str_upper", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.str_upper(args);
    });
    mgr.registerSystemFunction("util", "str_substring", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.str_substring(args);
    });
    mgr.registerSystemFunction("util", "str_length", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.str_length(args);
    });
}
#endif

// ============================================================
//  fox.std.socket
// ============================================================
#ifdef FOX_LIB_SOCKET
#include "../../libs/system/api/socket/SystemFunctionsSocket.h"
inline void register_socket(LibraryManager& mgr) {
    mgr.registerLibrary("socket");
    mgr.registerLibraryName("socket", "fox.std.socket");
    mgr.registerSystemFunction("socket", "socket_create", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_create(args);
    });
    mgr.registerSystemFunction("socket", "socket_connect", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_connect(args);
    });
    mgr.registerSystemFunction("socket", "socket_send", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_send(args);
    });
    mgr.registerSystemFunction("socket", "socket_recv", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_recv(args);
    });
    mgr.registerSystemFunction("socket", "socket_close", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_close(args);
    });
}
#endif

// ============================================================
//  fox.gl.fg  — OpenGL / GLFW graphics
// ============================================================
#ifdef FOX_LIB_GRAPHICS
#include "../../libs/graphics/SystemFunctionsGraphics.h"
inline void register_graphics(LibraryManager& mgr) {
    mgr.registerLibrary("gl_fg");
    mgr.registerLibraryName("gl_fg", "fox.gl.fg");
    mgr.registerSystemFunction("gl_fg", "create_window", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.create_window(args);
    });
    mgr.registerSystemFunction("gl_fg", "close", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.close(args);
    });
    mgr.registerSystemFunction("gl_fg", "window_should_close", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.window_should_close(args);
    });
    mgr.registerSystemFunction("gl_fg", "swap_buffers", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.swap_buffers(args);
    });
    mgr.registerSystemFunction("gl_fg", "poll_events", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.poll_events(args);
    });
    mgr.registerSystemFunction("gl_fg", "update", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.update(args);
    });
    mgr.registerSystemFunction("gl_fg", "clear_color", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.clear_color(args);
    });
    mgr.registerSystemFunction("gl_fg", "clear", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.clear(args);
    });
    mgr.registerSystemFunction("gl_fg", "draw_triangle", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.draw_triangle(args);
    });
    mgr.registerSystemFunction("gl_fg", "draw_rect", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.draw_rect(args);
    });
    mgr.registerSystemFunction("gl_fg", "draw_text", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.draw_text(args);
    });
    mgr.registerSystemFunction("gl_fg", "draw_line", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.draw_line(args);
    });
    mgr.registerSystemFunction("gl_fg", "draw_circle", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.draw_circle(args);
    });
    mgr.registerSystemFunction("gl_fg", "mouse_down", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.mouse_down(args);
    });
    mgr.registerSystemFunction("gl_fg", "mouse_pos", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.mouse_pos(args);
    });
}
#endif
