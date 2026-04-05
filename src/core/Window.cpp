#include "Window.h"
#include <stdexcept>
#include <iostream>

namespace bs{
    Window::Window(const WindowConfig& cfg){
        if(!glfwInit()){
            throw std::runtime_error("glfwInit failed");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        #ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        #endif
        m_handle = glfwCreateWindow(cfg.width, cfg.height, cfg.title.c_str(), nullptr, nullptr);
        if(!m_handle){
            throw std::runtime_error("glfwCreateWindow failed");
        }
        glfwMakeContextCurrent(m_handle);
        glfwSwapInterval(cfg.vsync ? 1 : 0); // Enable/disable vsync
        
        if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))){
            throw std::runtime_error("GLAD initialisation failed");
        }

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_handle, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
        glfwSetWindowUserPointer(m_handle, this);
        glfwSetFramebufferSizeCallback(m_handle, onFramebufferResize);
        
        std::cout << "[Window] OpenGL " << glGetString(GL_VERSION) 
                  << "| Framebuffer " << fbWidth << "x" << fbHeight << std::endl;
    }

    Window::~Window(){
        if(m_handle){
            glfwDestroyWindow(m_handle);
        }
        glfwTerminate();
    }

    bool Window::shouldClose() const {
        return glfwWindowShouldClose(m_handle);
    }
    void Window::swapBuffers(){
        glfwSwapBuffers(m_handle);
    }
    void Window::pollEvents(){
        glfwPollEvents();
    }

    void  Window::framebufferSize(int& w, int& h) const{
        glfwGetFramebufferSize(m_handle, &w, &h);
    }
    float Window::aspectRatio() const{
        int w, h;
        framebufferSize(w, h);
        return static_cast<float>(w) / static_cast<float>(h);
    }
    
    void Window::setResizeCallback(std::function<void(int, int)> cb){
        m_resizeCb = std::move(cb);
    }

    void Window::onFramebufferResize(GLFWwindow* handle, int w, int h){
        glViewport(0, 0, w, h);
        auto* win = static_cast<Window*>(glfwGetWindowUserPointer(handle));
        if(win->m_resizeCb){
            win->m_resizeCb(w, h);
        }
    }

} // namespace bs