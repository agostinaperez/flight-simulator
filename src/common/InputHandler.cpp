#include "InputHandler.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace {
//las velocidades se multiplican x el elapsed time

constexpr float kSurfaceRate = 1.1f; //velocidad a la q las teclas mueven elevador, alerones y timon
constexpr float kCenteringRate = 1.5f; //velocidad a lo q esos mandos vuelven a la posicion neutral al soltar las teclas
constexpr float kThrottleRate = 0.5f; //velocidad a la q cambia la potencia

} // namespace sin nombre para q solo se usen en este archivo

InputHandler::InputHandler() { //configuro los límites iniciales de los mandos, pasando de grados a radianes. 
    //set_limits puede sobreescribir esto
    min_elevator_ = glm::radians(-25.0f);
    max_elevator_ = glm::radians(15.0f);
    min_aileron_ = glm::radians(-20.0f);
    max_aileron_ = glm::radians(20.0f);
    max_rudder_ = glm::radians(30.0f);
}

//lo llamo 1 vez por cada cuadro
void InputHandler::update(GLFWwindow* window, float dt) {
    //dt es el tiempo transcurrido desde el cuadro anterior, en segundos. Me fijo q sea válido, y limito saltos bruscos
    //en los mandos desp de por ejemplo una pausa
    const float elapsed = std::isfinite(dt) ? std::clamp(dt, 0.0f, 0.25f) : 0.0f;

    //USO LOS CONTROLS DE LA LIBRERÍA dlfdm
    
    // WASD es el baston: W empuja (morro abajo), S tira (morro arriba).
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        controls_.elevator += kSurfaceRate * elapsed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        controls_.elevator -= kSurfaceRate * elapsed;
    }

    // A y D desplazan el baston lateralmente (alabeo).
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        controls_.aileron += kSurfaceRate * elapsed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        controls_.aileron -= kSurfaceRate * elapsed;
    }

    // Las flechas laterales mueven el timon; arriba/abajo cambian la potencia.
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        controls_.rudder += kSurfaceRate * elapsed;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        controls_.rudder -= kSurfaceRate * elapsed;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        controls_.throttle += kThrottleRate * elapsed;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        controls_.throttle -= kThrottleRate * elapsed;
    }

    // Auto-centrado: las superficies vuelven al neutro al soltar su tecla. La potencia NO se centra: se mantiene donde el piloto la dejo.
    //[&] es la función de captura de la lambda, e indica q puede usar variables locales de entorno por referencia, como
    //en este caso el elapsed. auto deja q c++ deduzca el tipo de la lambda
    auto centrar = [&](float& mando, float neutro) { //mando es una referencia, o sea puede modificar el control original
        //me fijo si estoy lo suficientemente cerca del neutro como para fijarlo ahí. Si sí, lo hago. Sino, lo voy acercando de a poco
        if (std::abs(mando - neutro) <= kCenteringRate * elapsed) {
            mando = neutro;
        } else {
            mando += (mando > neutro ? -1.0f : 1.0f) * kCenteringRate * elapsed;
        }
    };
    const bool elevator_mantenido = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    if (!elevator_mantenido) {
        centrar(controls_.elevator, neutral_.elevator);
    }

    const bool aileron_mantenido = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    if (!aileron_mantenido) {
        centrar(controls_.aileron, neutral_.aileron);
    }

    const bool rudder_mantenido = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    if (!rudder_mantenido) {
        centrar(controls_.rudder, neutral_.rudder);
    }

    // Los comandos quedan dentro de los limites del modelo de aeronave.
    controls_.elevator = std::clamp(controls_.elevator, min_elevator_, max_elevator_);
    controls_.aileron = std::clamp(controls_.aileron, min_aileron_, max_aileron_);
    controls_.rudder = std::clamp(controls_.rudder, -max_rudder_, max_rudder_);
    controls_.throttle = std::clamp(controls_.throttle, 0.0f, 1.0f);
}

void InputHandler::on_key(int key, int action) {
    // Los comandos discretos responden una sola vez al presionar la tecla.
    if (action == GLFW_PRESS && key == GLFW_KEY_SPACE) {
        paused_ = !paused_;
    }
}

void InputHandler::set_controls(const dlfdm::ControlInputs& controls) {
    controls_ = controls;
    controls_.elevator = std::clamp(controls_.elevator, min_elevator_, max_elevator_);
    controls_.aileron = std::clamp(controls_.aileron, min_aileron_, max_aileron_);
    controls_.rudder = std::clamp(controls_.rudder, -max_rudder_, max_rudder_);
    controls_.throttle = std::clamp(controls_.throttle, 0.0f, 1.0f);
}

void InputHandler::set_neutral(const dlfdm::ControlInputs& neutral) {
    neutral_ = neutral;
}

void InputHandler::set_limits(const dlfdm::AircraftParameters& parameters) {
    min_elevator_ = parameters.min_elevator;
    max_elevator_ = parameters.max_elevator;
    min_aileron_ = parameters.min_aileron;
    max_aileron_ = parameters.max_aileron;
    max_rudder_ = parameters.max_rudder;
    set_controls(controls_);
}
