#include "FlightData.h"
#include "GroundContact.h"
#include "InputHandler.h"

#include <GLFW/glfw3.h>
#include <dlfdm/fdmsolver.h>
#include <dlfdm/models/aircraft/jettrainer.h>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

bool cerca(float actual, float esperado, float tolerancia) {
    return std::abs(actual - esperado) <= tolerancia;
}

} // namespace

int main() {
    const dlfdm::AircraftParameters aircraft = dlfdm::jettrainer::load_model();
    const dlfdm::TrimPoint trim = dlfdm::jettrainer::get_trim_condition(
        dlfdm::jettrainer::TrimCondition::kISA5000TAS150);

    InputHandler input;
    input.set_limits(aircraft);
    input.set_controls(trim.controls);
    if (!cerca(input.controls().elevator, trim.controls.elevator, 1.0e-5f) ||
        !cerca(input.controls().throttle, trim.controls.throttle, 1.0e-5f)) {
        std::cerr << "El InputHandler no conservo los mandos del trim.\n";
        return 1;
    }

    dlfdm::ControlInputs outside_limits{};
    outside_limits.throttle = 2.0f;
    outside_limits.elevator = glm::radians(90.0f);
    outside_limits.aileron = glm::radians(-90.0f);
    outside_limits.rudder = glm::radians(90.0f);
    input.set_controls(outside_limits);
    if (!cerca(input.controls().throttle, 1.0f, 1.0e-5f) ||
        !cerca(input.controls().elevator, aircraft.max_elevator, 1.0e-5f) ||
        !cerca(input.controls().aileron, aircraft.min_aileron, 1.0e-5f) ||
        !cerca(input.controls().rudder, aircraft.max_rudder, 1.0e-5f)) {
        std::cerr << "El InputHandler no limito los mandos a los parametros.\n";
        return 1;
    }

    input.on_key(GLFW_KEY_SPACE, GLFW_PRESS);
    input.on_key(GLFW_KEY_SPACE, GLFW_REPEAT);
    if (!input.paused()) {
        std::cerr << "El comando de pausa no respeto el flanco de pulsacion.\n";
        return 1;
    }
    input.on_key(GLFW_KEY_SPACE, GLFW_PRESS);
    if (input.paused()) {
        std::cerr << "El comando de pausa no alterno al presionar nuevamente.\n";
        return 1;
    }

    dlfdm::AircraftState ejemplo{};
    ejemplo.inertial_position = glm::vec3(10.0f, 20.0f, -30.0f);
    ejemplo.phi = 0.1f;
    ejemplo.theta = 0.2f;
    ejemplo.psi = 0.3f;

    const FlightData mundo = to_world(ejemplo);
    if (!cerca(mundo.position.x, 20.0f, 1.0e-5f) ||
        !cerca(mundo.position.y, 30.0f, 1.0e-5f) ||
        !cerca(mundo.position.z, -10.0f, 1.0e-5f) ||
        !cerca(mundo.phi, -0.1f, 1.0e-5f) ||
        !cerca(mundo.theta, 0.2f, 1.0e-5f) ||
        !cerca(mundo.psi, -0.3f, 1.0e-5f)) {
        std::cerr << "Fallo en la conversion NED a coordenadas de mundo.\n";
        return 1;
    }

    FlightData heading{};
    heading.psi = -0.5f;
    const glm::vec3 forward = glm::vec3(
        orientacion_vuelo(heading) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
    if (forward.x <= 0.0f || forward.z >= 0.0f) {
        std::cerr << "La guiñada no orienta el morro hacia el rumbo esperado.\n";
        return 1;
    }

    const terreno::Alturas alturas(12345, 0.04f, 75.0f);
    constexpr unsigned int celdas = 16U;
    constexpr float paso = 16.0f;
    constexpr float altura_base = 4920.0f;
    const MeshData malla_terreno = terreno::grid(celdas, paso, alturas);
    const std::size_t indice_vertice = 6U * (celdas + 1U) + 8U;
    if (!cerca(terreno::height_at(celdas, alturas, 6.0f, 8.0f),
               malla_terreno.vertices[indice_vertice].position.y, 1.0e-5f)) {
        std::cerr << "La altura de contacto no coincide con un vertice del "
                     "terreno dibujado.\n";
        return 1;
    }

    const glm::vec2 origen(-128.0f, -128.0f);
    const float columna = 7.25f;
    const float fila = 5.75f;
    const float terreno_y = altura_base +
        terreno::height_at(celdas, alturas, fila, columna);
    const glm::vec3 posicion_mundo(
        origen.x + columna * paso,
        terreno_y + 1.0f,
        origen.y + fila * paso);

    dlfdm::AircraftState contacto{};
    contacto.inertial_position = glm::vec3(
        -posicion_mundo.z, posicion_mundo.x, -posicion_mundo.y);
    contacto.body_velocity = glm::vec3(100.0f, 0.0f, 4.0f);
    GroundContact suelo(alturas, celdas, paso, origen, altura_base);
    if (!suelo.resolve(contacto, 2.0f) ||
        !cerca(-contacto.inertial_position.z, terreno_y + 2.0f, 1.0e-4f) ||
        !cerca(contacto.body_velocity.z, 0.0f, 1.0e-4f)) {
        std::cerr << "El contacto no corrigio la penetracion ni la velocidad "
                     "hacia el terreno.\n";
        return 1;
    }

    dlfdm::AircraftState ascendiendo{};
    ascendiendo.inertial_position = contacto.inertial_position;
    ascendiendo.inertial_position.z = -(terreno_y + 1.0f);
    ascendiendo.body_velocity = glm::vec3(100.0f, 0.0f, -3.0f);
    if (!suelo.resolve(ascendiendo, 2.0f) ||
        !cerca(ascendiendo.body_velocity.z, -3.0f, 1.0e-4f)) {
        std::cerr << "El contacto impidio que el avion se aleje del suelo.\n";
        return 1;
    }

    dlfdm::FDMSolver solver(aircraft);
    solver.setState(trim.state);

    for (int step = 0; step < 60 * 120; ++step) {
        solver.update(trim.controls);
    }

    const dlfdm::AircraftState& state = solver.getState();
    const float altitude = -state.inertial_position.z;
    const float tas = glm::length(state.body_velocity);
    if (!std::isfinite(altitude) || !std::isfinite(tas) ||
        !cerca(altitude, 5000.0f, 20.0f) ||
        !cerca(tas, 150.0f, 5.0f)) {
        std::cerr << "El trim no se mantuvo: altitud=" << altitude
                  << " m, TAS=" << tas << " m/s.\n";
        return 1;
    }

    dlfdm::FDMSolver responsive_solver(aircraft);
    responsive_solver.setState(trim.state);
    dlfdm::ControlInputs aileron_input = trim.controls;
    aileron_input.aileron = glm::radians(5.0f);
    for (int step = 0; step < 120; ++step) {
        responsive_solver.update(aileron_input);
    }
    if (!std::isfinite(responsive_solver.getState().phi) ||
        std::abs(responsive_solver.getState().phi) < 1.0e-3f) {
        std::cerr << "El FDM no respondio al comando de aleron.\n";
        return 1;
    }

    dlfdm::FDMSolver pull_up_solver(aircraft);
    pull_up_solver.setState(trim.state);
    dlfdm::ControlInputs pull_up = trim.controls;
    pull_up.elevator = aircraft.min_elevator;
    for (int step = 0; step < 120; ++step) {
        pull_up_solver.update(pull_up);
    }
    if (!std::isfinite(pull_up_solver.getState().theta) ||
        pull_up_solver.getState().theta <= 0.0f) {
        std::cerr << "El elevador hacia arriba no genero cabeceo positivo.\n";
        return 1;
    }

    std::cout << "OK: mandos limitados, contacto con terreno, respuesta de "
                 "cabeceo y trim de 60 s "
              << "(altitud "
              << altitude << " m, TAS " << tas << " m/s).\n";
    return 0;
}
