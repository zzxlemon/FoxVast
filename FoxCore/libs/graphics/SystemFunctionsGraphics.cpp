#include "./SystemFunctionsGraphics.h"
#include <cstdio>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

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

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
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

static void orthoProj(float* m, float left, float right, float bottom, float top) {
    m[0] = 2.0f / (right - left); m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = 2.0f / (top - bottom); m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = -1.0f; m[11] = 0;
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = 0; m[15] = 1.0f;
}

Value FG::create_window(const std::vector<Value>& args) {
    if (args.size() != 3) {
        throw std::runtime_error("create_window: need 3 args (window_name, width, height)");
    }

    if (!glfwInit()) {
        throw std::runtime_error("create_window: failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    int w = args[1].asInt();
    int h = args[2].asInt();
    GLFWwindow* window = glfwCreateWindow(w, h, args[0].asString().c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("create_window: failed to create window.");
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

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

    FGWindow fgw;
    fgw.window = window;
    fgw.shaderProgram = program;
    fgw.VAO = VAO;
    fgw.VBO = VBO;
    fgw.width = w;
    fgw.height = h;
    fgw.textShaderProgram = textProgram;
    fgw.textVAO = textVAO;
    fgw.textVBO = textVBO;
    windows.push_back(fgw);

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
        for (auto& [key, atlas] : fgw.fontCache) {
            glDeleteTextures(1, &atlas.texture);
            free(atlas.cdata);
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
    FGWindow& fgw = windows.back();

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

    float proj[16];
    orthoProj(proj, 0.0f, (float)fgw.width, (float)fgw.height, 0.0f);

    glUseProgram(fgw.shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(fgw.shaderProgram, "projection"), 1, GL_FALSE, proj);
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
    FGWindow& fgw = windows.back();

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

    float proj[16];
    orthoProj(proj, 0.0f, (float)fgw.width, (float)fgw.height, 0.0f);

    glUseProgram(fgw.shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(fgw.shaderProgram, "projection"), 1, GL_FALSE, proj);
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
    FGWindow& fgw = windows.back();

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

    float proj[16];
    orthoProj(proj, 0.0f, (float)fgw.width, (float)fgw.height, 0.0f);

    glUseProgram(fgw.shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(fgw.shaderProgram, "projection"), 1, GL_FALSE, proj);
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
    FGWindow& fgw = windows.back();

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

    float proj[16];
    orthoProj(proj, 0.0f, (float)fgw.width, (float)fgw.height, 0.0f);

    glUseProgram(fgw.shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(fgw.shaderProgram, "projection"), 1, GL_FALSE, proj);
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
    FGWindow& fgw = windows.back();

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

    std::string cacheKey = fontFile + "@" + std::to_string(fontSize);

    // Load font if not cached
    if (fgw.fontCache.find(cacheKey) == fgw.fontCache.end()) {
        std::string fontPath = "FoxCore/native/fonts/" + fontFile;
        FILE* fp = fopen(fontPath.c_str(), "rb");
        if (!fp) {
            fontPath = "native/fonts/" + fontFile;
            fp = fopen(fontPath.c_str(), "rb");
        }
        if (!fp) {
            throw std::runtime_error("draw_text: cannot open font file '" + fontPath + "'");
        }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char* ttfBuffer = (unsigned char*)malloc(sz);
        fread(ttfBuffer, 1, sz, fp);
        fclose(fp);

        const int BITMAP_W = 512, BITMAP_H = 512;
        unsigned char* bitmap = (unsigned char*)malloc(BITMAP_W * BITMAP_H);

        stbtt_bakedchar* cdata = (stbtt_bakedchar*)malloc(sizeof(stbtt_bakedchar) * 96);
        int result = stbtt_BakeFontBitmap(ttfBuffer, 0, fontSize, bitmap, BITMAP_W, BITMAP_H, 32, 96, cdata);
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
    float proj[16];
    orthoProj(proj, 0.0f, (float)fgw.width, (float)fgw.height, 0.0f);

    glUseProgram(fgw.textShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(fgw.textShaderProgram, "projection"), 1, GL_FALSE, proj);
    glUniform3f(glGetUniformLocation(fgw.textShaderProgram, "textColor"), r, g, b);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas.texture);
    glUniform1i(glGetUniformLocation(fgw.textShaderProgram, "text"), 0);

    glBindVertexArray(fgw.textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fgw.textVBO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render each character
    float curX = x;
    float curY = y;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 32 || ch > 127) continue;

        stbtt_aligned_quad quad;
        stbtt_GetBakedQuad(cdata, atlas.bitmap_w, atlas.bitmap_h, ch - 32, &curX, &curY, &quad, 1);

        float vertices[24] = {
            quad.x0, quad.y0, quad.s0, quad.t0,
            quad.x1, quad.y0, quad.s1, quad.t0,
            quad.x0, quad.y1, quad.s0, quad.t1,
            quad.x1, quad.y0, quad.s1, quad.t0,
            quad.x1, quad.y1, quad.s1, quad.t1,
            quad.x0, quad.y1, quad.s0, quad.t1,
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);

    return Value();
}

// mouse_down: window_idx, button (0=left, 1=right, 2=middle)
// Returns 1 if pressed, 0 if not
Value FG::mouse_down(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("mouse_down: need 2 args (window, button)");
    }
    int idx = args[0].asInt();
    if (idx < 0 || idx >= static_cast<int>(windows.size())) {
        throw std::runtime_error("mouse_down: invalid window index");
    }
    int button = args[1].asInt();
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
    std::vector<Value> pos;
    pos.push_back(Value(static_cast<int>(x)));
    pos.push_back(Value(static_cast<int>(y)));
    return Value(pos);
}

