#pragma once

#include <glm/glm.hpp>
#include <dlfdm/defines.h>

#include "Terrain.h"

// Mantiene el centro de gravedad por encima de la superficie visual del terreno.
class GroundContact {
public:
    GroundContact(const terreno::Alturas& alturas, unsigned int celdas,
                  float paso, glm::vec2 origen_xz, float altura_base);

    // Corrige penetraciones y elimina solamente la velocidad dirigida hacia
    // abajo. Devuelve true mientras el avión está apoyado contra el terreno.
    bool resolve(dlfdm::AircraftState& estado, float despeje) const;

private:
    const terreno::Alturas& alturas_;
    unsigned int celdas_;
    float paso_;
    glm::vec2 origen_xz_;
    float altura_base_;
};
