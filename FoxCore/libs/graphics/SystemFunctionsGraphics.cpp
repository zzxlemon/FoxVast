#include "./SystemFunctionsGraphics.h"
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <cmath>
#include <windows.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO // we use stbi_load_from_memory with our own file IO
#include <stb_image.h>

//===============================
//
//     Fox Native Graphics (Modern OpenGL 3.3)
//            FG
//
//===============================

std::vector<FGWindow> FG::windows;

static const char* VERTEX_SHADER_SRC = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 ourColor;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(aPos, 1.0);
    ourColor = aColor;
}
)";

static const char* FRAGMENT_SHADER_SRC = R"(
#version 330 core
out vec4 FragColor;
in vec3 ourColor;
void main() {
    FragColor = vec4(ourColor, 1.0f);
}
)";

static const char* TEXT_VERTEX_SHADER_SRC = R"(
#version 330 core
layout (location = 0) in vec4 vertex; // vec2 pos + vec2 texcoord
out vec2 TexCoords;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

static const char* TEXT_FRAGMENT_SHADER_SRC = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec3 textColor;
void main() {
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
}
)";

static const char* IMAGE_VERTEX_SHADER_SRC = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTex;
out vec2 TexCoord;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTex;
}
)";

static const char* IMAGE_FRAGMENT_SHADER_SRC = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D tex;
uniform vec4 tint;
void main() {
    FragColor = texture(tex, TexCoord) * tint;
}
)";

// Recompute scale/offset for the logical-to-window mapping. Content is scaled
// to FIT the window (aspect preserved, fully visible) and anchored at the
// top-left corner; letterbox bars appear on the right/bottom only. The GL
// viewport always fills the window; the projection shifts the scene so the
// visible logical range is [offsetX/scale, width/scale + offsetX/scale].
static void updateViewport(FGWindow& fgw, int width, int height) {
    fgw.width = width;
    fgw.height = height;
    if (fgw.useLogicalScale && fgw.logicalW > 0 && fgw.logicalH > 0) {
        double xScale = (double)width / fgw.logicalW;
        double yScale = (double)height / fgw.logicalH;
        fgw.scale = std::min(xScale, yScale);
        fgw.offsetX = (fgw.logicalW * fgw.scale - width) * fgw.anchorX;
        fgw.offsetY = (fgw.logicalH * fgw.scale - height) * fgw.anchorY;
    } else {
        fgw.scale = 1.0;
        fgw.offsetX = 0.0;
        fgw.offsetY = 0.0;
    }
    glViewport(0, 0, width, height);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    FGWindow* fgw = static_cast<FGWindow*>(glfwGetWindowUserPointer(window));
    if (fgw) updateViewport(*fgw, width, height);
    else glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    FGWindow* fgw = static_cast<FGWindow*>(glfwGetWindowUserPointer(window));
    if (!fgw) return;
    if (action == GLFW_PRESS) fgw->pressedKeys.insert(key);
    else if (action == GLFW_RELEASE) fgw->releasedKeys.insert(key);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    FGWindow* fgw = static_cast<FGWindow*>(glfwGetWindowUserPointer(window));
    if (!fgw) return;
    fgw->scrollAccum += yoffset;
}

GLuint FG::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::string typeName = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        throw std::runtime_error(std::string("Shader compilation error (") + typeName + "): " + infoLog);
    }
    return shader;
}

GLuint FG::createShaderProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error(std::string("Shader link error: ") + infoLog);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLuint FG::createTextShaderProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, TEXT_VERTEX_SHADER_SRC);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, TEXT_FRAGMENT_SHADER_SRC);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error(std::string("Text shader link error: ") + infoLog);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLuint FG::createImageShaderProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, IMAGE_VERTEX_SHADER_SRC);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, IMAGE_FRAGMENT_SHADER_SRC);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error(std::string("Image shader link error: ") + infoLog);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

static TextureInfo findTexture(FGWindow& fgw, GLuint id) {
    for (auto& [key, info] : fgw.textureCache) {
        if (info.texture == id) return info;
    }
    throw std::runtime_error("invalid texture id (was it loaded with load_texture on this window?)");
}

// Whole-file read into memory (std::string may contain NUL bytes).
static std::string readFileBinary(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return std::string();
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::string data(sz, '\0');
    if (sz > 0) fread(&data[0], 1, sz, fp);
    fclose(fp);
    return data;
}

static void orthoProj(float* m, float left, float right, float bottom, float top) {
    m[0] = 2.0f / (right - left); m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = 2.0f / (top - bottom); m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = -1.0f; m[11] = 0;
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = 0; m[15] = 1.0f;
}

// Upload the orthographic projection for a program. With uniform scaling the
// projection shifts the scene by the viewport offset (negative offsets crop,
// positive ones leave letterbox bars outside the visible logical range).
static void setProj(FGWindow& fgw, GLuint program) {
    float m[16];
    if (fgw.useLogicalScale) {
        float left = (float)(fgw.offsetX / fgw.scale);
        float right = left + (float)(fgw.width / fgw.scale);
        float top = (float)(fgw.offsetY / fgw.scale);
        float bottom = top + (float)(fgw.height / fgw.scale);
        orthoProj(m, left, right, bottom, top);
    } else {
        orthoProj(m, 0.0f, (float)fgw.width, (float)fgw.height, 0.0f);
    }
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, m);
}

// key_down: window, key_name -> 1 while held, 0 otherwise.
// key_name: a single letter/digit, or one of: space enter escape tab backspace
//           up down left right home end pageup pagedown insert delete
//           shift ctrl alt f1..f12
static int keyNameToGlfw(const std::string& name) {
    if (name.size() == 1) {
        char c = (char)tolower((unsigned char)name[0]);
        if (c >= 'a' && c <= 'z') return GLFW_KEY_A + (c - 'a');
        if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
        // Printable symbols (e.g. '=', '-', '.', ',', '/') map 1:1 to
        // GLFW key codes, which follow ASCII for these.
        if (name[0] >= 33 && name[0] <= 126) return (int)name[0];
    }
    static const std::map<std::string, int> special = {
        {"space", GLFW_KEY_SPACE}, {"enter", GLFW_KEY_ENTER}, {"escape", GLFW_KEY_ESCAPE},
        {"tab", GLFW_KEY_TAB}, {"backspace", GLFW_KEY_BACKSPACE},
        {"up", GLFW_KEY_UP}, {"down", GLFW_KEY_DOWN}, {"left", GLFW_KEY_LEFT}, {"right", GLFW_KEY_RIGHT},
        {"home", GLFW_KEY_HOME}, {"end", GLFW_KEY_END}, {"pageup", GLFW_KEY_PAGE_UP},
        {"pagedown", GLFW_KEY_PAGE_DOWN}, {"insert", GLFW_KEY_INSERT}, {"delete", GLFW_KEY_DELETE},
        {"shift", GLFW_KEY_LEFT_SHIFT}, {"ctrl", GLFW_KEY_LEFT_CONTROL}, {"alt", GLFW_KEY_LEFT_ALT},
    };
    auto it = special.find(name);
    if (it != special.end()) return it->second;
    if (name.size() == 2 && (name[0] == 'f' || name[0] == 'F')) {
        int n = name[1] - '0';
        if (n >= 1 && n <= 9) return GLFW_KEY_F1 + (n - 1);
    }
    if (name.size() == 3 && (name[0] == 'f' || name[0] == 'F')) {
        int n = (name[1] - '0') * 10 + (name[2] - '0');
        if (n >= 10 && n <= 12) return GLFW_KEY_F1 + (n - 1);
    }
    throw std::runtime_error("key_down: unknown key '" + name + "'");
}

// Resolve a font file to real paths. Search order:
//   1. explicit path (contains ':' or '/' or '\') - used as-is
//   2. $FGFONT_DIR/<file>
//   3. <exe dir>/fonts/<file>          (distribution layout)
//   4. FoxCore/native/fonts/<file>     (dev layout, repo root as cwd)
//   5. native/fonts/<file>             (dev layout, FoxCore as cwd)
//   6. <file>                          (cwd)
static std::vector<std::string> fontSearchPaths(const std::string& file) {
    bool explicitPath = file.find(':') != std::string::npos ||
                        file.find('/') != std::string::npos ||
                        file.find('\\') != std::string::npos;
    std::vector<std::string> paths;
    if (explicitPath) {
        paths.push_back(file);
        return paths;
    }
    if (const char* dir = std::getenv("FGFONT_DIR"); dir && *dir) {
        paths.push_back(std::string(dir) + "/" + file);
    }
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (n > 0) {
        std::string exePath(exe, n);
        size_t slash = exePath.find_last_of("\\/");
        if (slash != std::string::npos) {
            paths.push_back(exePath.substr(0, slash) + "/fonts/" + file);
        }
    }
    paths.push_back("FoxCore/native/fonts/" + file);
    paths.push_back("native/fonts/" + file);
    paths.push_back(file);
    return paths;
}

Value FG::create_window(const std::vector<Value>& args) {
    if (args.size() != 3) {
        throw std::runtime_error("create_window: need 3 args (window_name, width, height)");
    }

    // The graphics library is single-window: all draw_* calls target the
    // window created here, so one GLFW context is always current.
    if (!windows.empty()) {
        throw std::runtime_error("create_window: only one window is supported");
    }

    if (!glfwInit()) {
        throw std::runtime_error("create_window: failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    int w = args[1].asInt();
    int h = args[2].asInt();
    GLFWwindow* window = glfwCreateWindow(w, h, args[0].asString().c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("create_window: failed to create window.");
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        throw std::runtime_error("create_window: failed to initialize GLAD.");
    }

    GLuint program = createShaderProgram();

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Position (vec3) at location 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color (vec3) at location 1
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glViewport(0, 0, w, h);

    GLuint textProgram = createTextShaderProgram();

    GLuint textVAO, textVBO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint imageProgram = createImageShaderProgram();

    GLuint imageVAO, imageVBO;
    glGenVertexArrays(1, &imageVAO);
    glGenBuffers(1, &imageVBO);
    glBindVertexArray(imageVAO);
    glBindBuffer(GL_ARRAY_BUFFER, imageVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    FGWindow fgw;
    fgw.window = window;
    fgw.shaderProgram = program;
    fgw.VAO = VAO;
    fgw.VBO = VBO;
    fgw.textShaderProgram = textProgram;
    fgw.textVAO = textVAO;
    fgw.textVBO = textVBO;
    fgw.imageShaderProgram = imageProgram;
    fgw.imageVAO = imageVAO;
    fgw.imageVBO = imageVBO;
    fgw.lastFrameTime = glfwGetTime();
    windows.push_back(fgw);
    glfwSetWindowUserPointer(window, &windows.back());
    FGWindow& cur = windows.back();
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    cur.width = fbW;
    cur.height = fbH;
    glViewport(0, 0, fbW, fbH);

    return Value(static_cast<int>(windows.size()) - 1);
}

Value FG::close(const std::vector<Value>& args) {
    for (auto& fgw : windows) {
        glDeleteVertexArrays(1, &fgw.VAO);
        glDeleteBuffers(1, &fgw.VBO);
        glDeleteProgram(fgw.shaderProgram);
        glDeleteVertexArrays(1, &fgw.textVAO);
        glDeleteBuffers(1, &fgw.textVBO);
        glDeleteProgram(fgw.textShaderProgram);
        glDeleteVertexArrays(1, &fgw.imageVAO);
        glDeleteBuffers(1, &fgw.imageVBO);
        glDeleteProgram(fgw.imageShaderProgram);
        for (auto& [key, atlas] : fgw.fontCache) {
            glDeleteTextures(1, &atlas.texture);
            free(atlas.cdata);
        }
        for (auto& [key, info] : fgw.textureCache) {
            glDeleteTextures(1, &info.texture);
        }
    }
    windows.clear();
    glfwTerminate();
    return Value();
}

Value FG::window_should_close(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("window_should_close: need 1 arg (window)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("window_should_close: invalid window index");
    }
    return Value(glfwWindowShouldClose(windows[idx].window));
}

Value FG::swap_buffers(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("swap_buffers: need 1 arg (window)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("swap_buffers: invalid window index");
    }
    glfwSwapBuffers(windows[idx].window);
    return Value();
}

Value FG::poll_events(const std::vector<Value>& args) {
    glfwPollEvents();
    return Value();
}

Value FG::update(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("update: need 1 arg (window)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("update: invalid window index");
    }
    glfwSwapBuffers(windows[idx].window);
    glfwPollEvents();
    double now = glfwGetTime();
    windows[idx].frameDt = now - windows[idx].lastFrameTime;
    windows[idx].lastFrameTime = now;
    return Value();
}

Value FG::clear_color(const std::vector<Value>& args) {
    if (args.size() != 4) {
        throw std::runtime_error("clear_color: need 4 args (r, g, b, a)");
    }
    glClearColor(args[0].asDouble(), args[1].asDouble(), args[2].asDouble(), args[3].asDouble());
    return Value();
}

Value FG::clear(const std::vector<Value>& args) {
    glClear(GL_COLOR_BUFFER_BIT);
    return Value();
}

// draw_triangle: x1, y1, r1, g1, b1,  x2, y2, r2, g2, b2,  x3, y3, r3, g3, b3
// Vertex format: [x, y, 0.0, r, g, b] per vertex (vec3 pos + vec3 color = 6 floats)
Value FG::draw_triangle(const std::vector<Value>& args) {
    if (args.size() != 15) {
        throw std::runtime_error("draw_triangle: need 15 args (3 vertices of x,y,r,g,b each)");
    }

    if (windows.empty()) return Value();
    FGWindow& fgw = windows[0];

    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };

    float vertices[18] = {
        toFloat(args[0]), toFloat(args[1]), 0.0f,
        toFloat(args[2]), toFloat(args[3]), toFloat(args[4]),

        toFloat(args[5]), toFloat(args[6]), 0.0f,
        toFloat(args[7]), toFloat(args[8]), toFloat(args[9]),

        toFloat(args[10]), toFloat(args[11]), 0.0f,
        toFloat(args[12]), toFloat(args[13]), toFloat(args[14]),
    };

    glUseProgram(fgw.shaderProgram);
    setProj(fgw, fgw.shaderProgram);
    glBindVertexArray(fgw.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    return Value();
}

// draw_rect: [x1,y1,r1,g1,b1], [x2,y2,r2,g2,b2], [x3,y3,r3,g3,b3], [x4,y4,r4,g4,b4]
// 4 array args, each vertex with own color, drawn as 2 triangles (6 vertices)
Value FG::draw_rect(const std::vector<Value>& args) {
    if (args.size() != 4) {
        throw std::runtime_error("draw_rect: need 4 args (arrays of [x,y,r,g,b] for each vertex)");
    }

    auto getVertex = [&](int idx) -> const std::vector<Value>& {
        if (args[idx].getType() != Value::Type::Array) {
            throw std::runtime_error("draw_rect: each arg must be an array");
        }
        const auto& arr = args[idx].asArray();
        if (arr.size() != 5) {
            throw std::runtime_error("draw_rect: each vertex array must have 5 elements (x,y,r,g,b)");
        }
        return arr;
    };

    if (windows.empty()) return Value();
    FGWindow& fgw = windows[0];

    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };

    const auto& v1 = getVertex(0);
    const auto& v2 = getVertex(1);
    const auto& v3 = getVertex(2);
    const auto& v4 = getVertex(3);

    float vertices[36] = {
        // Triangle 1: v1, v2, v4
        toFloat(v1[0]), toFloat(v1[1]), 0.0f,
        toFloat(v1[2]), toFloat(v1[3]), toFloat(v1[4]),

        toFloat(v2[0]), toFloat(v2[1]), 0.0f,
        toFloat(v2[2]), toFloat(v2[3]), toFloat(v2[4]),

        toFloat(v4[0]), toFloat(v4[1]), 0.0f,
        toFloat(v4[2]), toFloat(v4[3]), toFloat(v4[4]),

        // Triangle 2: v2, v3, v4
        toFloat(v2[0]), toFloat(v2[1]), 0.0f,
        toFloat(v2[2]), toFloat(v2[3]), toFloat(v2[4]),

        toFloat(v3[0]), toFloat(v3[1]), 0.0f,
        toFloat(v3[2]), toFloat(v3[3]), toFloat(v3[4]),

        toFloat(v4[0]), toFloat(v4[1]), 0.0f,
        toFloat(v4[2]), toFloat(v4[3]), toFloat(v4[4]),
    };

    glUseProgram(fgw.shaderProgram);
    setProj(fgw, fgw.shaderProgram);
    glBindVertexArray(fgw.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    return Value();
}

Value FG::draw_line(const std::vector<Value>& args) {
    if (args.size() != 8) {
        throw std::runtime_error("draw_line: need 8 args (x1, y1, x2, y2, thickness, r, g, b)");
    }
    if (windows.empty()) return Value();
    FGWindow& fgw = windows[0];

    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };

    float x1 = toFloat(args[0]), y1 = toFloat(args[1]);
    float x2 = toFloat(args[2]), y2 = toFloat(args[3]);
    float t  = toFloat(args[4]);
    float r  = toFloat(args[5]), g = toFloat(args[6]), b = toFloat(args[7]);

    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f) return Value();

    dx /= len;
    dy /= len;
    float px = -dy, py = dx;
    float h = t * 0.5f;

    float vertices[36] = {
        // Triangle 1: side1 start, side2 start, side1 end
        x1 + px * h, y1 + py * h, 0.0f, r, g, b,
        x1 - px * h, y1 - py * h, 0.0f, r, g, b,
        x2 + px * h, y2 + py * h, 0.0f, r, g, b,

        // Triangle 2: side2 start, side2 end, side1 end
        x1 - px * h, y1 - py * h, 0.0f, r, g, b,
        x2 - px * h, y2 - py * h, 0.0f, r, g, b,
        x2 + px * h, y2 + py * h, 0.0f, r, g, b,
    };

    glUseProgram(fgw.shaderProgram);
    setProj(fgw, fgw.shaderProgram);
    glBindVertexArray(fgw.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    return Value();
}

// draw_circle: cx, cy, radius, r, g, b
Value FG::draw_circle(const std::vector<Value>& args) {
    if (args.size() != 6) {
        throw std::runtime_error("draw_circle: need 6 args (cx, cy, radius, r, g, b)");
    }
    if (windows.empty()) return Value();
    FGWindow& fgw = windows[0];

    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };

    float cx = toFloat(args[0]), cy = toFloat(args[1]);
    float radius = toFloat(args[2]);
    float r = toFloat(args[3]), g = toFloat(args[4]), b = toFloat(args[5]);

    const int SEGMENTS = 64;
    std::vector<float> vertices;
    vertices.reserve((SEGMENTS + 2) * 6);

    // Center vertex
    vertices.push_back(cx); vertices.push_back(cy); vertices.push_back(0.0f);
    vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);

    for (int i = 0; i <= SEGMENTS; ++i) {
        float angle = 6.2831853f * i / SEGMENTS;
        float px = cx + radius * cosf(angle);
        float py = cy + radius * sinf(angle);
        vertices.push_back(px); vertices.push_back(py); vertices.push_back(0.0f);
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
    }

    glUseProgram(fgw.shaderProgram);
    setProj(fgw, fgw.shaderProgram);
    glBindVertexArray(fgw.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_FAN, 0, SEGMENTS + 2);
    glBindVertexArray(0);

    return Value();
}

Value FG::draw_text(const std::vector<Value>& args) {
    if (args.size() != 8) {
        throw std::runtime_error("draw_text: need 8 args (font_file, text, x, y, size, r, g, b)");
    }
    if (windows.empty()) return Value();
    FGWindow& fgw = windows[0];

    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };

    std::string fontFile = args[0].asString();
    std::string text = args[1].asString();
    float x = toFloat(args[2]);
    float y = toFloat(args[3]);
    float fontSize = toFloat(args[4]);
    float r = toFloat(args[5]);
    float g = toFloat(args[6]);
    float b = toFloat(args[7]);

    // Bake tier: 1x when drawing at native size (crisp 1:1 sampling), 2x/3x
    // when the window scale enlarges text, so it stays sharp. Tier switches
    // only shift glyph metrics by at most one font pixel (stbtt integer
    // rounding); the pen is scaled together with the glyphs (see below) so
    // there is no gross misplacement and no jump during resizes.
    int bakeMul = 1;
    if (fgw.useLogicalScale && fgw.scale > 1.0) {
        bakeMul = fgw.scale > 2.0 ? 3 : 2;
    }

    std::string cacheKey = fontFile + "@" + std::to_string(fontSize) + "@" + std::to_string(bakeMul);

    // Load font if not cached
    if (fgw.fontCache.find(cacheKey) == fgw.fontCache.end()) {
        FILE* fp = nullptr;
        for (const auto& p : fontSearchPaths(fontFile)) {
            fp = fopen(p.c_str(), "rb");
            if (fp) break;
        }
        if (!fp) {
            throw std::runtime_error("draw_text: cannot open font file '" + fontFile +
                                     "' (searched: FGFONT_DIR, <exe>/fonts/, FoxCore/native/fonts/, native/fonts/, cwd)");
        }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char* ttfBuffer = (unsigned char*)malloc(sz);
        fread(ttfBuffer, 1, sz, fp);
        fclose(fp);

        // Atlas large enough for 96 glyphs at this bake tier (512/1024/2048).
        const int BITMAP_W = 512 * bakeMul, BITMAP_H = 512 * bakeMul;
        unsigned char* bitmap = (unsigned char*)malloc(BITMAP_W * BITMAP_H);

        stbtt_bakedchar* cdata = (stbtt_bakedchar*)malloc(sizeof(stbtt_bakedchar) * 96);
        int result = stbtt_BakeFontBitmap(ttfBuffer, 0, fontSize * bakeMul, bitmap, BITMAP_W, BITMAP_H, 32, 96, cdata);
        if (result <= 0) {
            free(ttfBuffer);
            free(bitmap);
            free(cdata);
            throw std::runtime_error("draw_text: failed to bake font (no characters fit in atlas)");
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, BITMAP_W, BITMAP_H, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        FontAtlas atlas;
        atlas.texture = texture;
        atlas.cdata = cdata;
        atlas.bitmap_w = BITMAP_W;
        atlas.bitmap_h = BITMAP_H;
        fgw.fontCache[cacheKey] = atlas;

        free(ttfBuffer);
        free(bitmap);
    }

    FontAtlas& atlas = fgw.fontCache[cacheKey];
    stbtt_bakedchar* cdata = (stbtt_bakedchar*)atlas.cdata;

    // Build orthographic projection for pixel coordinates
    glUseProgram(fgw.textShaderProgram);
    setProj(fgw, fgw.textShaderProgram);
    glUniform3f(glGetUniformLocation(fgw.textShaderProgram, "textColor"), r, g, b);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas.texture);
    glUniform1i(glGetUniformLocation(fgw.textShaderProgram, "text"), 0);

    glBindVertexArray(fgw.textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.textVBO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render each character. The pen starts in bake-space (bakeMul x), the
    // vertex division below maps back to logical coordinates, keeping the
    // baseline exactly where draw_text was asked.
    float mf = (float)bakeMul;
    float curX = x * mf;
    float curY = y * mf;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 32 || ch > 127) continue;

        stbtt_aligned_quad quad;
        stbtt_GetBakedQuad(cdata, atlas.bitmap_w, atlas.bitmap_h, ch - 32, &curX, &curY, &quad, 1);

        float vertices[24] = {
            quad.x0 / mf, quad.y0 / mf, quad.s0, quad.t0,
            quad.x1 / mf, quad.y0 / mf, quad.s1, quad.t0,
            quad.x0 / mf, quad.y1 / mf, quad.s0, quad.t1,
            quad.x1 / mf, quad.y0 / mf, quad.s1, quad.t0,
            quad.x1 / mf, quad.y1 / mf, quad.s1, quad.t1,
            quad.x0 / mf, quad.y1 / mf, quad.s0, quad.t1,
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);

    return Value();
}

// Map a physical cursor position into logical coordinates (identity when
// uniform scaling is off).
static void cursorToLogical(FGWindow& fgw, double& x, double& y) {
    if (fgw.useLogicalScale) {
        x = (x + fgw.offsetX) / fgw.scale;
        y = (y + fgw.offsetY) / fgw.scale;
    }
}

// simulate_click: window, [times] -> queue left-button click events. Each
// queued click is consumed by the next mouse_down(win, 0) or button query,
// so an auto-clicker can drive game/UI logic without touching the cursor.
Value FG::simulate_click(const std::vector<Value>& args) {
    if (args.size() < 1 || args.size() > 2) {
        throw std::runtime_error("simulate_click: need 1-2 args (window, [times=1])");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("simulate_click: invalid window index");
    }
    int times = args.size() == 2 ? args[1].asInt() : 1;
    if (times < 1) return Value();
    windows[idx].fakeLeftClicks += times;
    return Value();
}

// mouse_down: window_idx, button (0=left, 1=right, 2=middle)
// Returns 1 if pressed, 0 if not. Simulated clicks (simulate_click) are
// consumed first.
Value FG::mouse_down(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("mouse_down: need 2 args (window, button)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("mouse_down: invalid window index");
    }
    int button = args[1].asInt();
    if (button == 0 && windows[idx].fakeLeftClicks > 0) {
        windows[idx].fakeLeftClicks--;
        return Value(1);
    }
    int glfwButton;
    switch (button) {
        case 0: glfwButton = GLFW_MOUSE_BUTTON_LEFT; break;
        case 1: glfwButton = GLFW_MOUSE_BUTTON_RIGHT; break;
        case 2: glfwButton = GLFW_MOUSE_BUTTON_MIDDLE; break;
        default: throw std::runtime_error("mouse_down: button must be 0 (left), 1 (right), or 2 (middle)");
    }
    int state = glfwGetMouseButton(windows[idx].window, glfwButton);
    return Value(state == GLFW_PRESS ? 1 : 0);
}

// mouse_pos: window_idx
// Returns [x, y] with cursor position in pixel coords (0,0 = top-left)
Value FG::mouse_pos(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("mouse_pos: need 1 arg (window)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("mouse_pos: invalid window index");
    }
    double x, y;
    glfwGetCursorPos(windows[idx].window, &x, &y);
    FGWindow& fgw = windows[idx];
    if (fgw.useLogicalScale) {
        x = (x + fgw.offsetX) / fgw.scale;
        y = (y + fgw.offsetY) / fgw.scale;
        if (x < 0.0) x = 0.0;
        if (y < 0.0) y = 0.0;
        if (x > (double)fgw.logicalW) x = (double)fgw.logicalW;
        if (y > (double)fgw.logicalH) y = (double)fgw.logicalH;
    }
    std::vector<Value> pos;
    pos.push_back(Value(static_cast<int>(x)));
    pos.push_back(Value(static_cast<int>(y)));
    return Value(pos);
}

// Draw a solid rect through the shared geometry program in one call.
// verts: [x0,y0 r,g,b] [x1,y0] [x0,y1] / [x1,y0] [x1,y1] [x0,y1]
static void fillRect(FGWindow& fgw, float x0, float y0, float x1, float y1,
                     float r, float g, float b) {
    float vertices[36] = {
        x0, y0, r, g, b,
        x1, y0, r, g, b,
        x0, y1, r, g, b,
        x1, y0, r, g, b,
        x1, y1, r, g, b,
        x0, y1, r, g, b,
    };
    glUseProgram(fgw.shaderProgram);
    setProj(fgw, fgw.shaderProgram);
    glBindVertexArray(fgw.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// button: window, x, y, w, h, font, text -> draws a push button and returns 1
// once per click while the cursor is inside it (0 otherwise). Simulated
// clicks from simulate_click work too, so an auto-clicker can press UI.
Value FG::button(const std::vector<Value>& args) {
    if (args.size() != 7) {
        throw std::runtime_error("button: need 7 args (window, x, y, w, h, font, text)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("button: invalid window index");
    }
    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };
    FGWindow& fgw = windows[idx];
    float x = toFloat(args[1]);
    float y = toFloat(args[2]);
    float w = toFloat(args[3]);
    float h = toFloat(args[4]);
    std::string font = args[5].asString();
    std::string text = args[6].asString();

    double mx, my;
    glfwGetCursorPos(fgw.window, &mx, &my);
    cursorToLogical(fgw, mx, my);
    bool hover = mx >= x && mx <= x + w && my >= y && my <= y + h;

    bool down = glfwGetMouseButton(fgw.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool click = down && !fgw.prevLeftDown && hover;
    if (fgw.fakeLeftClicks > 0 && hover) {
        fgw.fakeLeftClicks--;
        click = true;
    }
    fgw.prevLeftDown = down;

    float r, g, b;
    if (down && hover) { r = 0.16f; g = 0.18f; b = 0.22f; }
    else if (hover)    { r = 0.32f; g = 0.36f; b = 0.44f; }
    else               { r = 0.24f; g = 0.27f; b = 0.33f; }
    fillRect(fgw, x, y, x + w, y + h, r, g, b);

    float size = h * 0.55f;
    float textW = (float)text.size() * size * 0.6f;
    float tx = x + (w - textW) / 2.0f;
    if (tx < x + 6.0f) tx = x + 6.0f;
    float ty = y + (h - size) / 2.0f + size * 0.78f;
    this->draw_text(std::vector<Value>{
        Value(font), Value(text), Value(tx), Value(ty), Value(size),
        Value(0.92f), Value(0.92f), Value(0.92f)});

    return Value(click ? 1 : 0);
}

// text_input: window, x, y, w, h, font, size -> draws a text box and returns
// its current text. Clicking focuses it; typing appends ASCII characters,
// Backspace deletes, Enter submits: the full line is returned and cleared
// (chat-style). While focused, pressed keystrokes are consumed by the box.
Value FG::text_input(const std::vector<Value>& args) {
    if (args.size() != 7) {
        throw std::runtime_error("text_input: need 7 args (window, x, y, w, h, font, size)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("text_input: invalid window index");
    }
    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };
    FGWindow& fgw = windows[idx];
    fgw.tiEnabled = true;
    fgw.tiX = toFloat(args[1]);
    fgw.tiY = toFloat(args[2]);
    fgw.tiW = toFloat(args[3]);
    fgw.tiH = toFloat(args[4]);
    fgw.tiFont = args[5].asString();
    fgw.tiSize = toFloat(args[6]);

    double mx, my;
    glfwGetCursorPos(fgw.window, &mx, &my);
    cursorToLogical(fgw, mx, my);
    bool inside = mx >= fgw.tiX && mx <= fgw.tiX + fgw.tiW &&
                  my >= fgw.tiY && my <= fgw.tiY + fgw.tiH;
    bool down = glfwGetMouseButton(fgw.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (down && !fgw.prevLeftDown) {
        fgw.tiActive = inside;
    }
    fgw.prevLeftDown = down;

    std::string submit;
    if (fgw.tiActive) {
        for (int key : fgw.pressedKeys) {
            if (key >= 32 && key <= 126) {
                fgw.tiText.push_back((char)key);
            } else if (key == GLFW_KEY_BACKSPACE && !fgw.tiText.empty()) {
                fgw.tiText.pop_back();
            } else if (key == GLFW_KEY_ENTER) {
                submit = fgw.tiText;
                fgw.tiText.clear();
            }
        }
        fgw.pressedKeys.clear();
    }
    fgw.tiCursorT += fgw.frameDt;

    // Background + border
    fillRect(fgw, fgw.tiX, fgw.tiY, fgw.tiX + fgw.tiW, fgw.tiY + fgw.tiH,
             0.10f, 0.11f, 0.14f);
    float bc = fgw.tiActive ? 1.0f : 0.3f;
    fillRect(fgw, fgw.tiX, fgw.tiY - 1.0f, fgw.tiX + fgw.tiW, fgw.tiY, 0.25f * bc, 0.75f * bc, bc);
    fillRect(fgw, fgw.tiX, fgw.tiY + fgw.tiH, fgw.tiX + fgw.tiW, fgw.tiY + fgw.tiH + 1.0f, 0.25f * bc, 0.75f * bc, bc);

    // Visible text: right-aligned tail that fits the box
    float size = fgw.tiSize;
    float charW = size * 0.6f;
    int maxChars = (int)((fgw.tiW - 10.0f) / charW);
    if (maxChars < 1) maxChars = 1;
    std::string shown = fgw.tiText;
    if ((int)shown.size() > maxChars) shown = shown.substr(shown.size() - maxChars);
    float ty = fgw.tiY + (fgw.tiH - size) / 2.0f + size * 0.78f;
    float tx = fgw.tiX + 5.0f;
    this->draw_text(std::vector<Value>{
        Value(fgw.tiFont), Value(shown), Value(tx), Value(ty), Value(size),
        Value(1.0f), Value(1.0f), Value(1.0f)});

    // Cursor caret (blinks)
    if (fgw.tiActive && std::fmod(fgw.tiCursorT, 1.0) < 0.5) {
        float cx = tx + (float)shown.size() * charW;
        fillRect(fgw, cx, fgw.tiY + 4.0f, cx + 2.0f, fgw.tiY + fgw.tiH - 4.0f,
                 0.9f, 0.9f, 0.9f);
    }

    if (!submit.empty()) return Value(submit);
    return Value(fgw.tiText);
}

// Shared implementation for set_scale / set_scale_anchor.
static Value setScaleImpl(FGWindow& fgw, int lw, int lh) {
    if (lw <= 0 || lh <= 0) {
        fgw.useLogicalScale = false;
        fgw.logicalW = 0;
        fgw.logicalH = 0;
    } else {
        fgw.useLogicalScale = true;
        fgw.logicalW = lw;
        fgw.logicalH = lh;
    }
    updateViewport(fgw, fgw.width, fgw.height);
    return Value();
}

// set_scale: window, logical_w, logical_h -> enable uniform scaling of all
// content. All drawing coordinates are then interpreted in the logical
// resolution and scaled to FIT the window (aspect preserved), anchored at the
// top-left corner; letterbox bars appear on the right/bottom when the window
// aspect differs. Pass (0, 0) to turn the scaling off and go back to raw
// pixel coordinates.
Value FG::set_scale(const std::vector<Value>& args) {
    if (args.size() != 3) {
        throw std::runtime_error("set_scale: need 3 args (window, logical_w, logical_h)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("set_scale: invalid window index");
    }
    return setScaleImpl(windows[idx], args[1].asInt(), args[2].asInt());
}

// set_scale_anchor: window, logical_w, logical_h, anchor_x, anchor_y -> same
// as set_scale but with a custom anchor (0 = left/top, 1 = right/bottom,
// 0.5 = centered). Anchor 0 keeps the top-left corner fixed: resizes only
// change the letterbox bars, coordinates never shift.
Value FG::set_scale_anchor(const std::vector<Value>& args) {
    if (args.size() != 5) {
        throw std::runtime_error("set_scale_anchor: need 5 args (window, logical_w, logical_h, anchor_x, anchor_y)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("set_scale_anchor: invalid window index");
    }
    double ax = args[3].asDouble();
    double ay = args[4].asDouble();
    if (ax < 0.0 || ax > 1.0 || ay < 0.0 || ay > 1.0) {
        throw std::runtime_error("set_scale_anchor: anchors must be in [0, 1]");
    }
    FGWindow& fgw = windows[idx];
    fgw.anchorX = ax;
    fgw.anchorY = ay;
    return setScaleImpl(fgw, args[1].asInt(), args[2].asInt());
}

// key_down: window, key_name -> 1 while held, 0 otherwise.
// key_name: a single letter/digit, or one of: space enter escape tab backspace
//           up down left right home end pageup pagedown insert delete
//           shift ctrl alt f1..f12
Value FG::key_down(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("key_down: need 2 args (window, key_name)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("key_down: invalid window index");
    }
    int glfwKey = keyNameToGlfw(args[1].asString());
    int state = glfwGetKey(windows[idx].window, glfwKey);
    return Value(state == GLFW_PRESS ? 1 : 0);
}

// key_pressed: window, key_name -> 1 once per press while the key is held.
// Edge-triggered: an event is reported at most once per physical press and
// consumed by the query (missing it means losing that event).
Value FG::key_pressed(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("key_pressed: need 2 args (window, key_name)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("key_pressed: invalid window index");
    }
    int glfwKey = keyNameToGlfw(args[1].asString());
    return Value(windows[idx].pressedKeys.erase(glfwKey) > 0 ? 1 : 0);
}

// key_released: window, key_name -> 1 once per release, event is consumed.
Value FG::key_released(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("key_released: need 2 args (window, key_name)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("key_released: invalid window index");
    }
    int glfwKey = keyNameToGlfw(args[1].asString());
    return Value(windows[idx].releasedKeys.erase(glfwKey) > 0 ? 1 : 0);
}

// mouse_wheel: window -> accumulated vertical scroll since last query (consumed).
Value FG::mouse_wheel(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("mouse_wheel: need 1 arg (window)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("mouse_wheel: invalid window index");
    }
    double acc = windows[idx].scrollAccum;
    windows[idx].scrollAccum = 0.0;
    return Value(acc);
}

// window_size: window -> [width, height] in pixels (live, follows resize).
Value FG::window_size(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("window_size: need 1 arg (window)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("window_size: invalid window index");
    }
    std::vector<Value> size;
    size.push_back(Value(windows[idx].width));
    size.push_back(Value(windows[idx].height));
    return Value(size);
}

// time: current time in seconds since window creation (glfwGetTime).
Value FG::time(const std::vector<Value>& args) {
    if (!args.empty()) {
        throw std::runtime_error("time: need 0 args");
    }
    return Value(glfwGetTime());
}

// frame_time: window -> seconds the previous frame took (updated by update).
Value FG::frame_time(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("frame_time: need 1 arg (window)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("frame_time: invalid window index");
    }
    return Value(windows[idx].frameDt);
}

// key_any_pressed: window -> 1 if any key went down since the last query,
// then consumes all pending press events. Handy for "press any key" prompts.
Value FG::key_any_pressed(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("key_any_pressed: need 1 arg (window)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("key_any_pressed: invalid window index");
    }
    if (!windows[idx].pressedKeys.empty()) {
        windows[idx].pressedKeys.clear();
        return Value(1);
    }
    return Value(0);
}

// load_texture: window, file -> texture id. PNG/JPG/BMP/GIF via stb_image.
// Textures are cached per file path; the cache is freed by close().
// Returns the id so a texture can be drawn with draw_image*.
Value FG::load_texture(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("load_texture: need 2 args (window, file)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("load_texture: invalid window index");
    }
    FGWindow& fgw = windows[idx];
    std::string file = args[1].asString();
    auto it = fgw.textureCache.find(file);
    if (it != fgw.textureCache.end()) {
        return Value(static_cast<int>(it->second.texture));
    }
    if (fgw.textureCache.size() >= 256) {
        throw std::runtime_error("load_texture: texture cache full (256 entries)");
    }
    std::string data = readFileBinary(file);
    if (data.empty()) {
        throw std::runtime_error("load_texture: cannot open image file '" + file + "'");
    }
    int w = 0, h = 0, comp = 0;
    unsigned char* pixels = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(data.data()), static_cast<int>(data.size()), &w, &h, &comp, 4);
    if (!pixels) {
        throw std::runtime_error(std::string("load_texture: cannot decode image '" + file + "': ") +
                                 (stbi_failure_reason() ? stbi_failure_reason() : "unknown error"));
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
    TextureInfo info;
    info.texture = tex;
    info.width = w;
    info.height = h;
    fgw.textureCache[file] = info;
    return Value(static_cast<int>(tex));
}

// draw_image: window, texture_id, x, y, w, h -> draw the texture, no rotation.
Value FG::draw_image(const std::vector<Value>& args) {
    if (args.size() != 6) {
        throw std::runtime_error("draw_image: need 6 args (window, texture, x, y, w, h)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("draw_image: invalid window index");
    }
    FGWindow& fgw = windows[idx];
    findTexture(fgw, static_cast<GLuint>(args[1].asInt()));
    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };
    float x = toFloat(args[2]);
    float y = toFloat(args[3]);
    float w = toFloat(args[4]);
    float h = toFloat(args[5]);

    float vertices[24] = {
        x,     y,     0.0f, 0.0f,
        x + w, y,     1.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,
        x,     y,     0.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,
        x,     y + h, 0.0f, 1.0f,
    };
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(fgw.imageShaderProgram);
    setProj(fgw, fgw.imageShaderProgram);
    glUniform4f(glGetUniformLocation(fgw.imageShaderProgram, "tint"), 1.0f, 1.0f, 1.0f, 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(args[1].asInt()));
    glUniform1i(glGetUniformLocation(fgw.imageShaderProgram, "tex"), 0);
    glBindVertexArray(fgw.imageVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.imageVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    return Value();
}

// draw_image_rotated: window, texture, x, y, w, h, deg -> rotate around center.
Value FG::draw_image_rotated(const std::vector<Value>& args) {
    if (args.size() != 7) {
        throw std::runtime_error("draw_image_rotated: need 7 args (window, texture, x, y, w, h, deg)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("draw_image_rotated: invalid window index");
    }
    FGWindow& fgw = windows[idx];
    findTexture(fgw, static_cast<GLuint>(args[1].asInt()));
    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };
    float x = toFloat(args[2]);
    float y = toFloat(args[3]);
    float w = toFloat(args[4]);
    float h = toFloat(args[5]);
    float deg = toFloat(args[6]);

    float rad = deg * 0.01745329252f;
    float c = cosf(rad), s = sinf(rad);
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    // corners relative to center: TL, TR, BR, BL
    float corners[4][2] = {
        {-w * 0.5f, -h * 0.5f},
        { w * 0.5f, -h * 0.5f},
        { w * 0.5f,  h * 0.5f},
        {-w * 0.5f,  h * 0.5f},
    };
    float p[4][2];
    for (int i = 0; i < 4; ++i) {
        p[i][0] = cx + corners[i][0] * c - corners[i][1] * s;
        p[i][1] = cy + corners[i][0] * s + corners[i][1] * c;
    }
    float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    float vertices[24] = {
        p[0][0], p[0][1], uv[0][0], uv[0][1],
        p[1][0], p[1][1], uv[1][0], uv[1][1],
        p[2][0], p[2][1], uv[2][0], uv[2][1],
        p[0][0], p[0][1], uv[0][0], uv[0][1],
        p[2][0], p[2][1], uv[2][0], uv[2][1],
        p[3][0], p[3][1], uv[3][0], uv[3][1],
    };
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(fgw.imageShaderProgram);
    setProj(fgw, fgw.imageShaderProgram);
    glUniform4f(glGetUniformLocation(fgw.imageShaderProgram, "tint"), 1.0f, 1.0f, 1.0f, 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(args[1].asInt()));
    glUniform1i(glGetUniformLocation(fgw.imageShaderProgram, "tex"), 0);
    glBindVertexArray(fgw.imageVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.imageVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    return Value();
}

// draw_image_tinted: window, texture, x, y, w, h, r, g, b, a -> tint color
// multiplies the texture (white 1,1,1,1 = unchanged, alpha fades the image).
Value FG::draw_image_tinted(const std::vector<Value>& args) {
    if (args.size() != 10) {
        throw std::runtime_error("draw_image_tinted: need 10 args (window, texture, x, y, w, h, r, g, b, a)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("draw_image_tinted: invalid window index");
    }
    FGWindow& fgw = windows[idx];
    findTexture(fgw, static_cast<GLuint>(args[1].asInt()));
    auto toFloat = [](const Value& v) -> float {
        if (v.getType() == Value::Type::Int) return static_cast<float>(v.asInt());
        return static_cast<float>(v.asDouble());
    };
    float x = toFloat(args[2]);
    float y = toFloat(args[3]);
    float w = toFloat(args[4]);
    float h = toFloat(args[5]);
    float r = toFloat(args[6]);
    float g = toFloat(args[7]);
    float b = toFloat(args[8]);
    float a = toFloat(args[9]);

    float vertices[24] = {
        x,     y,     0.0f, 0.0f,
        x + w, y,     1.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,
        x,     y,     0.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,
        x,     y + h, 0.0f, 1.0f,
    };
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(fgw.imageShaderProgram);
    setProj(fgw, fgw.imageShaderProgram);
    glUniform4f(glGetUniformLocation(fgw.imageShaderProgram, "tint"), r, g, b, a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(args[1].asInt()));
    glUniform1i(glGetUniformLocation(fgw.imageShaderProgram, "tex"), 0);
    glBindVertexArray(fgw.imageVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.imageVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    return Value();
}

// image_size: window, texture -> [width, height] in pixels.
Value FG::image_size(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("image_size: need 2 args (window, texture)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("image_size: invalid window index");
    }
    TextureInfo info = findTexture(windows[idx], static_cast<GLuint>(args[1].asInt()));
    std::vector<Value> size;
    size.push_back(Value(info.width));
    size.push_back(Value(info.height));
    return Value(size);
}

