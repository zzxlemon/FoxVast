#pragma once

#include "../../src/interpreter/interpreter.hpp"
#include <glfw3.h>


//===============================
//
//     Fox Native Graphics 
//            FG
//
//===============================

// Now this "Fox Vative Graphics" lib of the don't good

// This function use the set window size of call back.
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

class FG {
public:
    Value create_window(const std::vector<Value>& args);
    Value close(const std::vector<Value>& args);
    Value window_should_close(const std::vector<Value>& args);
    Value swap_buffers(const std::vector<Value>& args);
    Value poll_events(const std::vector<Value>& args);
    Value update(const std::vector<Value>& args); 
    Value clear_color(const std::vector<Value>& args);
    Value clear(const std::vector<Value>& args);
    Value draw_triangle(const std::vector<Value>& args);
    Value begin(const std::vector<Value>& args);
    Value end(const std::vector<Value>& args);
private:
    static std::vector<GLFWwindow*> gw;
};