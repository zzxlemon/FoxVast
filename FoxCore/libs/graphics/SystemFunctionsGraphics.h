#pragma once

#include "../../src/interpreter/interpreter.hpp"
#include <glad/glad.h>
#include <glfw3.h>
#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <set>

struct FontAtlas {
    GLuint texture;
    void* cdata; // stbtt_bakedchar[96]
    int bitmap_w;
    int bitmap_h;
};

struct TextureInfo {
    GLuint texture;
    int width;
    int height;
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

    // Texture rendering (images)
    GLuint imageShaderProgram;
    GLuint imageVAO;
    GLuint imageVBO;
    std::map<std::string, TextureInfo> textureCache;

    // Input events (edge-triggered, consumed on query)
    std::set<int> pressedKeys;
    std::set<int> releasedKeys;
    double scrollAccum = 0.0;

    // UI widgets
    bool prevLeftDown = false;   // edge detection for button / text_input
    int fakeLeftClicks = 0;      // queue of simulated clicks (auto-clicker)
    bool tiEnabled = false;      // a text input box is active
    bool tiActive = false;       // the text input has keyboard focus
    std::string tiText;
    double tiCursorT = 0.0;      // cursor blink timer
    float tiX = 0, tiY = 0, tiW = 0, tiH = 0, tiSize = 20;
    std::string tiFont;

    // Timing
    double lastFrameTime = 0.0;
    double frameDt = 0.0;

// Uniform scaling: content is drawn in a fixed logical resolution and scaled
// to FIT the window, aspect preserved, anchored at the top-left corner
// (letterbox bars appear on the right/bottom when the aspect differs).
bool useLogicalScale = false;
int logicalW = 0;
int logicalH = 0;
double scale = 1.0;
double offsetX = 0.0;
double offsetY = 0.0;
// Anchor point of the scaled content within the window, in [0,1] per axis.
// Defaults: X = 0.5 (letterbox bars split evenly left/right), Y = 0.0
// (content pinned to the top, bars only at the bottom, y never drifts).
// set_scale_anchor can override either axis.
double anchorX = 0.5;
double anchorY = 0.0;
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
    Value key_down(const std::vector<Value>& args);
    Value key_pressed(const std::vector<Value>& args);
    Value key_released(const std::vector<Value>& args);
    Value key_any_pressed(const std::vector<Value>& args);
    Value mouse_wheel(const std::vector<Value>& args);
    Value window_size(const std::vector<Value>& args);
    Value time(const std::vector<Value>& args);
    Value frame_time(const std::vector<Value>& args);
    Value load_texture(const std::vector<Value>& args);
    Value draw_image(const std::vector<Value>& args);
    Value draw_image_rotated(const std::vector<Value>& args);
    Value draw_image_tinted(const std::vector<Value>& args);
    Value image_size(const std::vector<Value>& args);
    Value set_scale(const std::vector<Value>& args);
    Value set_scale_anchor(const std::vector<Value>& args);
    Value button(const std::vector<Value>& args);
    Value text_input(const std::vector<Value>& args);
    Value simulate_click(const std::vector<Value>& args);
private:
    static std::vector<FGWindow> windows;
    static GLuint compileShader(GLenum type, const char* source);
    static GLuint createShaderProgram();
    static GLuint createTextShaderProgram();
    static GLuint createImageShaderProgram();
};
