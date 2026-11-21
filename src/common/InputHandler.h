#pragma once

#include <dlfdm/defines.h>

struct GLFWwindow;

// Convierte el estado del teclado en mandos limitados para el FDM.
class InputHandler {
public:
    InputHandler();

    // Integra las teclas mantenidas usando el tiempo del cuadro. Las
    // superficies recorren su rango completo en menos de medio segundo.
    void update(GLFWwindow* window, float dt);

    // Alterna la pausa solo en el flanco de presion, no durante GLFW_REPEAT.
    void on_key(int key, int action);

    const dlfdm::ControlInputs& controls() const { return controls_; }
    bool paused() const { return paused_; }

    void set_controls(const dlfdm::ControlInputs& controls);
    void set_limits(const dlfdm::AircraftParameters& parameters);

    // Fija el neutro (posicion de trim) al que vuelven las superficies cuando
    // se suelta la tecla. La potencia no se auto-centra: queda donde se la deja.
    void set_neutral(const dlfdm::ControlInputs& neutral);

private:
    dlfdm::ControlInputs controls_{};  // Controles actuales; inicia todo en cero.
    dlfdm::ControlInputs neutral_{};   // Referencia de neutro (trim).
    float min_elevator_ = 0.0f;
    float max_elevator_ = 0.0f;
    float min_aileron_ = 0.0f;
    float max_aileron_ = 0.0f;
    float max_rudder_ = 0.0f;
    bool paused_ = false;
};
