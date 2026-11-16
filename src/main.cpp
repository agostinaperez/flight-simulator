#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <iostream>

namespace {

// El tamano inicial del framebuffer; GLFW avisara si la ventana cambia de tamano.
constexpr int kAncho = 1280;
constexpr int kAlto = 720;

// GLFW entrega errores por callback para que no queden ocultos.
void informar_error_glfw(int codigo, const char* descripcion) {
    std::cerr << "[GLFW] Error " << codigo << ": " << descripcion << '\n';
}

// Se ajusta el viewport al tamano real del framebuffer, no solo al de la ventana.
void ajustar_viewport(GLFWwindow*, int ancho, int alto) {
    glViewport(0, 0, ancho, alto);
}

} // namespace

int main() {
    // Instalo el callback antes de iniciar GLFW para recibir errores de arranque.
    glfwSetErrorCallback(informar_error_glfw);

    // Si GLFW no arranca, no hay ventana ni contexto donde dibujar.
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "No se pudo inicializar GLFW.\n";
        return 1;
    }

    // Solicito OpenGL moderno antes de crear la ventana y su contexto.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // La ventana es el destino visible y aloja el contexto de OpenGL.
    GLFWwindow* ventana = glfwCreateWindow(
        kAncho, kAlto, "Flight Simulator", nullptr, nullptr);
    if (ventana == nullptr) {
        std::cerr << "No se pudo crear la ventana OpenGL.\n";
        glfwTerminate();
        return 1;
    }

    // El contexto debe ser el actual antes de cargar GLAD o llamar a OpenGL.
    glfwMakeContextCurrent(ventana);
    // GLAD resuelve las funciones de OpenGL del driver para este contexto.
    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "No se pudieron cargar las funciones de OpenGL con GLAD.\n";
        glfwDestroyWindow(ventana);
        glfwTerminate();
        return 1;
    }

    // Mantengo el area de dibujo sincronizada con el tamano del framebuffer.
    glfwSetFramebufferSizeCallback(ventana, ajustar_viewport);
    // Espero al refresco del monitor para evitar intercambios excesivos de cuadros.
    glfwSwapInterval(1);
    glViewport(0, 0, kAncho, kAlto);

    // Cada vuelta limpia el cuadro anterior, lo presenta y procesa eventos.
    while (glfwWindowShouldClose(ventana) == GLFW_FALSE) {
        glClearColor(0.35f, 0.62f, 0.82f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwSwapBuffers(ventana);
        glfwPollEvents();
    }

    // Destruyo la ventana antes de cerrar GLFW para liberar el contexto ordenadamente.
    glfwDestroyWindow(ventana);
    glfwTerminate();
    return 0;
}
