#include "./SystemFunctionsGraphics.h"

//===============================
//
//     Fox Native Graphics 
//            FG
//
//===============================

int gw_number = 0;
std::vector<GLFWwindow*> FG::gw; 

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

Value FG::create_window(const std::vector<Value>& args) {
    if (args.size() != 3){
        throw std::runtime_error("create_window: need 2 args (window_name, width, height)");
    }

    if (!glfwInit()) {
        throw std::runtime_error("create_window: failed to initialize GLFW.");
        exit(-1);
        return -1;
    }

    // Configuration window prompts
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // This line is required for MacOS.
#endif

    GLFWwindow* window = glfwCreateWindow(args[1].asInt(), args[2].asInt(), args[0].asString().c_str(), NULL, NULL);
    if (!window) {
        throw std::runtime_error("create_window: failed to create window.");
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glShadeModel(GL_SMOOTH);

    FG fg;
    fg.gw.push_back(window);

    if (gw_number == 0){
        return Value(gw_number);
    }

    gw_number++;

    return Value(gw_number);  
}

Value FG::close(const std::vector<Value>& args) {   
    glfwTerminate();
    return Value();
}

Value FG::window_should_close(const std::vector<Value>& args) {
    if (args.size() != 1){
        throw std::runtime_error("window_should_close: need 1 args (window)");
    }
    FG fg;
    if (fg.gw.empty()) {
        throw std::runtime_error("window_should_close: you must initialize and create a window instance before passing it to this function.");
    }

    return Value(glfwWindowShouldClose(fg.gw.at(args[0].asInt())));
}

Value FG::swap_buffers(const std::vector<Value>& args){
    if (args.size() != 1){
        throw std::runtime_error("swap_buffers: need 1 args (window)");
    }

    FG fg;
    if (fg.gw.empty()) {
        throw std::runtime_error("swap_buffers: you must initialize and create a window instance before passing it to this function.");
    }

    glfwSwapBuffers(fg.gw.at(args[0].asInt()));

    return Value();
}

Value FG::poll_events(const std::vector<Value>& args){
    glfwPollEvents();

    return Value();
}

Value FG::update(const std::vector<Value>& args){
    if (args.size() != 1){
        throw std::runtime_error("update: need 1 args (window)");
    }

    FG fg;
    if (fg.gw.empty()) {
        throw std::runtime_error("update: you must initialize and create a window instance before passing it to this function.");
    }

    glfwSwapBuffers(fg.gw.at(args[0].asInt()));
    glfwPollEvents();

    return Value();
} 

Value FG::clear_color(const std::vector<Value>& args){
    if (args.size() != 4){
        throw std::runtime_error("clear_color: need 4 args (r, g, b, a)");
    }

    glClearColor(args[0].asDouble(), args[1].asDouble(), args[2].asDouble(), args[3].asDouble());
    return Value();
}

Value FG::clear(const std::vector<Value>& args){
    glClear(GL_COLOR_BUFFER_BIT);
    return Value();
}


// x, y r, g, b 
// x1, y1 r1, g1, b1
// x2, y2 r2, g2, b2
Value FG::draw_triangle(const std::vector<Value>& args){
    if (args.size() != 15){
        throw std::runtime_error("draw_triangle: need 15 args ([x, y, r, g, b], [x1, y1, r1, g1, b1], [x2, y2, r2, g2, b2])"); 
    }

    glColor3f(args[2].asDouble(), args[3].asDouble(), args[4].asDouble());
    glVertex2f(args[0].asDouble(),  args[1].asDouble());

    glColor3f(args[7].asDouble(), args[8].asDouble(), args[9].asDouble());
    glVertex2f(args[5].asDouble(),  args[6].asDouble());

    glColor3f(args[12].asDouble(), args[13].asDouble(), args[14].asDouble());
    glVertex2f(args[10].asDouble(),  args[11].asDouble());

    return Value();
}

Value FG::begin(const std::vector<Value>& args){
    glBegin(GL_TRIANGLES);
    return Value();
}

Value FG::end(const std::vector<Value>& args){
    glEnd();
    return Value();
}