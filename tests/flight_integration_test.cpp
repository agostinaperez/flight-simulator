#include "CameraSystem.h"
#include "FlightData.h"
#include "GroundContact.h"
#include "InputHandler.h"

#include <GLFW/glfw3.h>
#include <dlfdm/fdmsolver.h>
#include <dlfdm/models/aircraft/jettrainer.h>

#include <glm/gtc/matrix_inverse.hpp>

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

    // --- Sistema de camaras: tres vistas y redimensionado ---------------------
    // Helpers para extraer de la matriz de VISTA la posicion del ojo y la
    // direccion a la que mira la camara (el -Z del espacio de camara, llevado
    // al mundo con la inversa de la vista).
    auto eye = [](const glm::mat4& view) {
        return glm::vec3(glm::inverse(view)[3]);
    };
    auto dir_vista = [](const glm::mat4& view) {
        return glm::vec3(glm::inverse(view) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
    };

    // Avion recto y nivelado: orientacion identidad, morro hacia -Z.
    const FlightData actitud{};
    const glm::mat4 orientacion = orientacion_vuelo(actitud);
    const glm::vec3 pos(100.0f, 5000.0f, -50.0f);
    const glm::vec3 morro(0.0f, 0.0f, -1.0f);

    CameraSystem camara(1280, 720);
    if (camara.mode() != CameraMode::Orbital) {
        std::cerr << "La camara no arranca en vista orbital.\n";
        return 1;
    }

    // Vista orbital: la camara mira al avion (su forward apunta hacia el avion).
    camara.update(pos, orientacion, CameraCommand{});
    {
        const glm::vec3 hacia_avion = glm::normalize(
            pos - eye(camara.data().view));
        const glm::vec3 vista = glm::normalize(dir_vista(camara.data().view));
        if (glm::dot(vista, hacia_avion) < 0.99f) {
            std::cerr << "La vista orbital no apunta al avion.\n";
            return 1;
        }
    }

    // Vista de persecucion: la camara queda atras del avion y mira hacia delante.
    camara.cycle_mode();
    if (camara.mode() != CameraMode::Chase) {
        std::cerr << "cycle_mode no avanzo a la vista de persecucion.\n";
        return 1;
    }
    camara.update(pos, orientacion, CameraCommand{});
    {
        const glm::vec3 hacia_camara = eye(camara.data().view) - pos;
        if (glm::dot(hacia_camara, morro) >= 0.0f) {
            std::cerr << "La vista de persecucion no queda detras del avion.\n";
            return 1;
        }
        const glm::vec3 vista = glm::normalize(dir_vista(camara.data().view));
        if (glm::dot(vista, morro) < 0.5f) {
            std::cerr << "La vista de persecucion no mira hacia delante.\n";
            return 1;
        }
    }

    // Vista de cabina: el ojo queda dentro de la aeronave y mira al morro.
    camara.cycle_mode();
    if (camara.mode() != CameraMode::Cockpit) {
        std::cerr << "cycle_mode no avanzo a la vista de cabina.\n";
        return 1;
    }
    camara.update(pos, orientacion, CameraCommand{});
    {
        if (glm::distance(eye(camara.data().view), pos) > 3.0f) {
            std::cerr << "La vista de cabina no queda dentro de la aeronave.\n";
            return 1;
        }
        const glm::vec3 vista = glm::normalize(dir_vista(camara.data().view));
        if (glm::dot(vista, morro) < 0.99f) {
            std::cerr << "La vista de cabina no mira hacia el morro.\n";
            return 1;
        }
    }

    // Con una actitud de maniobra (cabeceo y alabeo) las vistas de persecucion y
    // cabina siguen definidas: ninguna matriz de vista queda con entradas no
    // finitas (la orientacion nunca alinea morro con arriba, ver decision [4]).
    {
        FlightData giro{};
        giro.theta = 0.7f;
        giro.phi = 0.6f;
        const glm::mat4 orient_giro = orientacion_vuelo(giro);

        // Cabina (modo actual) con actitud de maniobra.
        camara.update(pos, orient_giro, CameraCommand{});
        // Persecucion con actitud de maniobra.
        camara.cycle_mode();  // Cockpit -> Orbital
        camara.cycle_mode();  // Orbital -> Chase
        camara.update(pos, orient_giro, CameraCommand{});

        const glm::mat4 view = camara.data().view;
        for (int fila = 0; fila < 4; ++fila) {
            for (int col = 0; col < 4; ++col) {
                if (!std::isfinite(view[fila][col])) {
                    std::cerr << "Una vista quedo indefinida durante la maniobra.\n";
                    return 1;
                }
            }
        }
    }

    // cycle_mode recorre las tres vistas y vuelve al principio.
    camara.cycle_mode();  // Chase -> Cockpit
    camara.cycle_mode();  // Cockpit -> Orbital
    if (camara.mode() != CameraMode::Orbital) {
        std::cerr << "cycle_mode no volvio a la vista orbital.\n";
        return 1;
    }

    // Redimensionado: la proyeccion actualiza el aspect con el nuevo tamaño.
    camara.set_viewport(640, 480);
    const float aspect = camara.data().projection[1][1] /
                         camara.data().projection[0][0];
    if (!cerca(aspect, 640.0f / 480.0f, 1.0e-4f)) {
        std::cerr << "El redimensionado no actualizo el aspect de la proyeccion.\n";
        return 1;
    }

    std::cout << "OK: mandos limitados, contacto con terreno, respuesta de "
                 "cabeceo y trim de 60 s "
              << "(altitud "
              << altitude << " m, TAS " << tas << " m/s).\n";
    return 0;
}
