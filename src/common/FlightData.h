#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <dlfdm/defines.h>

struct FlightData {
    glm::vec3 position{0.0f};
    float phi = 0.0f;
    float theta = 0.0f;
    float psi = 0.0f;
};

// Convierte la posición NED (norte, este, abajo) a OpenGL (derecha, arriba,
// profundidad). Se invierten dos ejes para conservar un sistema dextrógiro.
inline FlightData to_world(const dlfdm::AircraftState& ned) {
    FlightData world;
    // El este pasa a X, abajo se niega para obtener arriba y norte pasa a -Z.
    world.position.x = ned.inertial_position.y;
    world.position.y = -ned.inertial_position.z;
    world.position.z = -ned.inertial_position.x;
    // Los signos de los ángulos reflejan los ejes transformados; pitch conserva
    // el signo. Así las matrices usan los ejes positivos del sistema OpenGL.
    world.phi = -ned.phi;
    world.theta = ned.theta;
    world.psi = -ned.psi;
    return world;
}

// Compone la actitud 3-2-1: guiñada, cabeceo y finalmente alabeo.
inline glm::mat4 orientacion_vuelo(const FlightData& flight) {
    glm::mat4 orientation(1.0f);
    // En GLM, las llamadas sucesivas se aplican de derecha a izquierda.
    orientation = glm::rotate(
        orientation, flight.psi, glm::vec3(0.0f, 1.0f, 0.0f));
    orientation = glm::rotate(
        orientation, flight.theta, glm::vec3(1.0f, 0.0f, 0.0f));
    orientation = glm::rotate(
        orientation, flight.phi, glm::vec3(0.0f, 0.0f, 1.0f));
    return orientation;
}
