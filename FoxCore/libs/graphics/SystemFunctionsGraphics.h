#pragma once

#include "../../src/interpreter/interpreter.hpp"
#include <glad/glad.h>
#include <glfw3.h>
#include <string>
#include <iostream>
#include <vector>
#include <map>

struct FontAtlas {
    GLuint texture;
    void* cdata; // stbtt_bakedchar[96]
    int bitmap_w;
    int bitmap_h;
};

struct FGWindow {
    GLFWwindow* window;
    GLuint shaderProgram;
    GLuint VAO;
    GLuint VBO;
    int width;
    int height;

    // Text rendering
    GLuint textShaderProgram;
    GLuint textVAO;
    GLuint textVBO;
    std::map<std::string, FontAtlas> fontCache;
};

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
    Value draw_rect(const std::vector<Value>& args);
    Value draw_line(const std::vector<Value>& args);
    Value draw_circle(const std::vector<Value>& args);
    Value draw_text(const std::vector<Value>& args);
    Value mouse_down(const std::vector<Value>& args);
    Value mouse_pos(const std::vector<Value>& args);
private:
    static std::vector<FGWindow> windows;
    static GLuint compileShader(GLenum type, const char* source);
    static GLuint createShaderProgram();
    static GLuint createTextShaderProgram();
};
