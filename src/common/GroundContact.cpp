#include "GroundContact.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "FlightData.h"

GroundContact::GroundContact(const terreno::Alturas& alturas,
                             unsigned int celdas, float paso,
                             glm::vec2 origen_xz, float altura_base)
    : alturas_(alturas),
      celdas_(celdas),
      paso_(paso),
      origen_xz_(origen_xz),
      altura_base_(altura_base) {
    if (celdas_ == 0U || !std::isfinite(paso_) || paso_ <= 0.0f) {
        throw std::invalid_argument(
            "GroundContact: la grilla debe tener celdas y paso positivo.");
    }
}

bool GroundContact::resolve(dlfdm::AircraftState& estado,
                            float despeje) const {
    if (!std::isfinite(despeje) || despeje < 0.0f) {
        throw std::invalid_argument(
            "GroundContact::resolve: el despeje debe ser finito y no negativo.");
    }

    const FlightData posicion = to_world(estado);
    if (!std::isfinite(posicion.position.x) ||
        !std::isfinite(posicion.position.y) ||
        !std::isfinite(posicion.position.z)) {
        throw std::runtime_error(
            "GroundContact::resolve: la posición del avión no es finita.");
    }
    const float columna = (posicion.position.x - origen_xz_.x) / paso_;
    const float fila = (posicion.position.z - origen_xz_.y) / paso_;
    const float maximo = static_cast<float>(celdas_);
    const bool dentro_de_grilla =
        columna >= 0.0f && columna <= maximo &&
        fila >= 0.0f && fila <= maximo;
    const float desnivel = dentro_de_grilla
        ? terreno::height_at(celdas_, alturas_, fila, columna)
        : 0.0f;
    const float altura_suelo = altura_base_ + desnivel;
    const float altura_minima = altura_suelo + despeje;

    if (posicion.position.y >= altura_minima) {
        return false;
    }

    // El FDM almacena posición NED: z positiva apunta hacia abajo.
    estado.inertial_position.z = -altura_minima;

    // Se proyecta la velocidad corporal a NED, se cancela el componente que
    // penetra el suelo y se vuelve al marco del avión.
    const float cp = std::cos(estado.phi);
    const float sp = std::sin(estado.phi);
    const float ct = std::cos(estado.theta);
    const float st = std::sin(estado.theta);
    const float cy = std::cos(estado.psi);
    const float sy = std::sin(estado.psi);
    const glm::mat3 cuerpo_a_ned(
        ct * cy, sp * st * cy - cp * sy, cp * st * cy + sp * sy,
        ct * sy, sp * st * sy + cp * cy, cp * st * sy - sp * cy,
        -st,     sp * ct,                cp * ct);
    glm::vec3 velocidad_ned = cuerpo_a_ned * estado.body_velocity;

    if (velocidad_ned.z > 0.0f) {
        velocidad_ned.z = 0.0f;
        estado.body_velocity = glm::transpose(cuerpo_a_ned) * velocidad_ned;
    }
    return true;
}
