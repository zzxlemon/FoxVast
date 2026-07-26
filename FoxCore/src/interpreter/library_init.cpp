#include "library_manager.hpp"
#include "../util/common.hpp"
// lib include
#include "../../libs/system/random/SystemFunctionsRandom.h"
#include "../../libs/system/fs/SystemFunctionsFile.h"
#include "../../libs/system/math/SystemFunctionsMath.h"
#include "../../libs/system/io/util/SystemFunctionUtil.h"
#include "../../libs/system/api/socket/SystemFunctionsSocket.h"
#include "../../libs/graphics/SystemFunctionsGraphics.h"

void initSystemLibraries() {
    auto& libMgr = LibraryManager::getInstance();

    libMgr.registerLibrary("random");
    libMgr.registerLibraryName("random", "fox.std.random");
    libMgr.registerSystemFunction("random", "random", [](const std::vector<Value>& args) -> Value {
        Random random;
        return random.RandomStart(args);
        });

    libMgr.registerLibrary("file");
    libMgr.registerLibraryName("file", "fox.sys.io.fs");
    libMgr.registerSystemFunction("file", "file_open", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileOpen(args);
        });
    libMgr.registerSystemFunction("file", "file_read", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileRead(args);
        });
    libMgr.registerSystemFunction("file", "file_write", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileWrite(args);
        });
    libMgr.registerSystemFunction("file", "file_close", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileClose(args);
        });
    libMgr.registerSystemFunction("file", "file_remove", [](const std::vector<Value>& args) -> Value {
        File file;
        return file.FileDelete(args);
        });
    
    // Register Math lib
	libMgr.registerLibrary("math");
    libMgr.registerLibraryName("math", "fox.std.math");
    libMgr.registerSystemFunction("math", "sin", [](const std::vector<Value>& args) -> Value {
        Math math;
        return math.sin(args);
		});
    libMgr.registerSystemFunction("math", "cos", [](const std::vector<Value>& args)->Value {
        Math math;
        return math.cos(args);
	});
    libMgr.registerSystemFunction("math", "tan", [](const std::vector<Value>& args) -> Value {
        Math math;
        return math.tan(args);
        });

    // Register util lib
    libMgr.registerLibrary("util");
    libMgr.registerLibraryName("util", "fox.std.util");
    libMgr.registerSystemFunction("util", "length", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.length(args);
        });
    libMgr.registerSystemFunction("util", "IntChangeString", [](const std::vector<Value>& args) -> Value {
        Util util;  
        return util.IntChangeString(args);
        });
    libMgr.registerSystemFunction("util", "StringChangeInt", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.StringChangeInt(args);
        });
    libMgr.registerSystemFunction("util", "StringChangeDouble", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.StringChangeDouble(args);
        });
    libMgr.registerSystemFunction("util", "DoubleChangeString", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.DoubleChangeString(args);
        });
    libMgr.registerSystemFunction("util", "DoubleChangeInt", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.DoubleChangeInt(args);
        });
    libMgr.registerSystemFunction("util", "IntChangeDouble", [](const std::vector<Value>& args) -> Value {
        Util util;
        return util.IntChangeDouble(args);
        });

    libMgr.registerLibrary("socket");
    libMgr.registerLibraryName("socket", "fox.std.socket");
    libMgr.registerSystemFunction("socket", "socket_create", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_create(args);
        });
    libMgr.registerSystemFunction("socket", "socket_connect", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_connect(args);
        });
    libMgr.registerSystemFunction("socket", "socket_send", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_send(args);
        });
    libMgr.registerSystemFunction("socket", "socket_recv", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_recv(args);
        });
    libMgr.registerSystemFunction("socket", "socket_close", [](const std::vector<Value>& args) -> Value {
        Socket sock;
        return sock.socket_close(args);
        });

    libMgr.registerLibrary("gl_fg");
    libMgr.registerLibraryName("gl_fg", "fox.gl.fg");
    libMgr.registerSystemFunction("gl_fg", "create_window", [](const std::vector<Value>& args)  -> Value {
        FG fg;
        return fg.create_window(args);
    }); 
    libMgr.registerSystemFunction("gl_fg", "close", [](const std::vector<Value>& args)  -> Value {
        FG fg;
        return fg.close(args);
    }); 
    libMgr.registerSystemFunction("gl_fg", "window_should_close", [](const std::vector<Value>& args)  -> Value {
        FG fg;
        return fg.window_should_close(args);
    }); 
    libMgr.registerSystemFunction("gl_fg", "swap_buffers", [](const std::vector<Value>& args)  -> Value {
        FG fg;
        return fg.swap_buffers(args);
    }); 
    libMgr.registerSystemFunction("gl_fg", "poll_events", [](const std::vector<Value>& args)  -> Value {
        FG fg;
        return fg.poll_events(args);
    }); 
    libMgr.registerSystemFunction("gl_fg", "update", [](const std::vector<Value>& args)  -> Value {
        FG fg;
        return fg.update(args);
    });
    libMgr.registerSystemFunction("gl_fg", "clear_color", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.clear_color(args);
    });
    libMgr.registerSystemFunction("gl_fg", "clear", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.clear(args);
    });
    libMgr.registerSystemFunction("gl_fg", "draw_triangle", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.draw_triangle(args);
    });
    libMgr.registerSystemFunction("gl_fg", "draw_rect", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.draw_rect(args);
    });
    libMgr.registerSystemFunction("gl_fg", "draw_text", [](const std::vector<Value>& args) -> Value {
        FG fg;
        return fg.draw_text(args);
    });
}