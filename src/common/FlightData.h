#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <dlfdm/defines.h>

struct FlightData {
    glm::vec3 position{0.0f};
    float phi = 0.0f; //alabeo
    float theta = 0.0f; //cabeceo
    float psi = 0.0f; // guiñada
};

// Convierte la posición NED (norte, este, abajo) a OpenGL (derecha, arriba, profundidad). Se invierten dos ejes para conservar un sistema dextrógiro.
//no cambio al ned, sino q creo un objeto nuevo world
inline FlightData to_world(const dlfdm::AircraftState& ned) { //AircraftState es un tipo de dato  q viene con la biblioteca dlfdm
    FlightData world;
    /*Opengl hace q X= derecha, Y= arriba, Z = profundidad, entonces tengo q asignar en X la y del NED (este), 
    en la Y la z del NED (abajo, negado pq yo necesito "arriba") y en profundidad  Z es la -X, porque a + Norte, menos Z*/
    world.position.x = ned.inertial_position.y;
    world.position.y = -ned.inertial_position.z;
    world.position.z = -ned.inertial_position.x;
    // Los signos de los ángulos reflejan los ejes transformados; pitch conserva el signo. Así las matrices usan los ejes positivos del sistema OpenGL.
    world.phi = -ned.phi;
    world.theta = ned.theta;
    world.psi = -ned.psi;
    return world;
}

// uso los ángulos q convertí en el flightdata para calcular cómo voy a rotar el modelo.
inline glm::mat4 orientacion_vuelo(const FlightData& flight) {
    glm::mat4 orientation(1.0f); //matriz identidad, todavía no rota ni nada

    /* rotate recibe (matriz base, angulo en radianes, eje) y devuelve una nueva matriz c la rotación agregada*/
    orientation = glm::rotate(orientation, flight.psi, glm::vec3(0.0f, 1.0f, 0.0f)); //guiñada alrededor de Y
    orientation = glm::rotate(orientation, flight.theta, glm::vec3(1.0f, 0.0f, 0.0f)); //cabeceo alrededor de X
    orientation = glm::rotate(orientation, flight.phi, glm::vec3(0.0f, 0.0f, 1.0f)); //alabeo alrededor de Z
    // En GLM, las llamadas sucesivas se aplican de derecha a izquierda (phi -> theta -> psi)
    return orientation; //devuelvo la matriz de rotación posta
}
