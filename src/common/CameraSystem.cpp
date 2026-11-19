// CameraSystem.cpp — implementación de la cámara orbital.
// No toca OpenGL: solo matemática de matrices (glm). Por eso se puede probar
// fuera del loop de render (ver tests/ si hiciera falta).

#include "CameraSystem.h"

#include <glm/gtc/matrix_transform.hpp>  // glm::lookAt / perspective / radians
#include <cmath>                         // std::sin / std::cos
#include <algorithm>                     // std::min / std::max (acotes)
#include <iostream>                      // std::cout (reporte inicial)

// -----------------------------------------------------------------------------
// Configuración de la órbita (todo en un namespace ANÓNIMO: solo se ve acá).
// -----------------------------------------------------------------------------
namespace {

// Limite del pitch: 0.9 * pi/2. Evita que la cámara quede EXACTAMENTE sobre el
// eje vertical, caso en el que la dirección de vista y el vector "up" quedan
// colineales y el producto vectorial de lookAt se anula (matriz indefinida).
constexpr float kMaxPitch = 0.9f * 1.57079632679f;  // ~81° (0.9 * pi/2)

// Acotes de la distancia al objetivo. Un mínimo > 0 para que la cámara no
// traspase al avión, y un máximo para no perderlo de vista al alejarse.
constexpr float kDistMin = 2.0f;
constexpr float kDistMax = 6000.0f;

// Vector "arriba" del mundo para lookAt. La escena usa Y hacia arriba.
const glm::vec3 kUp(0.0f, 1.0f, 0.0f);

} // fin del namespace anónimo (nada se exporta)

// -----------------------------------------------------------------------------
// Constructor: guarda el tamaño inicial y arma la proyección inicial.
// -----------------------------------------------------------------------------
CameraSystem::CameraSystem(int width, int height) {
    set_viewport(width, height);   // calcula aspect + proyección inicial
    std::cout << "[CameraSystem] camara orbital inicializada\n";
}

// -----------------------------------------------------------------------------
// set_viewport: recalcula la PROYECCIÓN cuando cambia el tamaño de la ventana.
// -----------------------------------------------------------------------------
void CameraSystem::set_viewport(int width, int height) {
    // DECISIÓN [2]: al minimizar, GLFW puede reportar alto 0 (o negativo). Con
    // alto 0 el aspect = width/0 quedaría indefinido (división por cero) y la
    // proyección rota. Acá se usa 1 como valor "de guardia": la proyección
    // queda definida (aunque de aspecto deforme); al restaurar la ventana se
    // recalcula bien. Este es el lugar correcto de la comprobación: es el
    // módulo que CONOCE el aspect y el único que debería vigilarlo.
    const int alto_seguro = (height > 0) ? height : 1;

    // aspect = ancho / alto del framebuffer. De él sale el ángulo de visión
    // horizontal: tan(θw/2) = aspect · tan(θh/2).
    const float aspect = static_cast<float>(width) / static_cast<float>(alto_seguro);

    // glm::perspective(fovy, aspect, near, far). fovy va en RADIANES.
    // near y far definen el volumen de visión (frustum) en unidades de escena:
    // lo que quede fuera de [near, far] se recorta (clipping).
    data_.projection = glm::perspective(fovy_, aspect, near_, far_);
}

// -----------------------------------------------------------------------------
// update: acumula los deltas del usuario y recalcula la matriz de VISTA.
// -----------------------------------------------------------------------------
void CameraSystem::update(glm::vec3 pos_avion, glm::vec3 angulos_avion,
                          const CameraCommand& cmd) {
    // La cámara orbital no usa la actitud del avión (siempre mira al objetivo);
    // el parámetro se recibe para compatibilidad con vistas futuras (ver
    // decisión [1] del header). El (void) evita el warning de parámetro sin uso.
    (void)angulos_avion;

    // 1) ACUMULAR los deltas sobre el estado interno de la órbita.
    yaw_       += cmd.yaw_delta;
    pitch_     += cmd.pitch_delta;
    distancia_ += cmd.dist_delta;

    // 2) ACOTAR el estado (no dejar que crezca sin límites).
    pitch_     = std::max(-kMaxPitch, std::min(kMaxPitch, pitch_));   // decisión [4]
    distancia_ = std::max(kDistMin,   std::min(kDistMax,   distancia_));

    // 3) Pasaje de esféricas a cartesianas para la posición de la cámara (eye).
    //    Con los ángulos definidos como en la guía:
    //      x = d·cos(θ)·cos(ψ)   (θ = pitch, ψ = yaw)
    //      y = d·sin(θ)          (vertical)
    //      z = d·cos(θ)·sin(ψ)
    //    El eye sale de sumarle ese offset al objetivo: la cámara siempre orbita
    //    alrededor del punto al que mira.
    const float x = distancia_ * std::cos(pitch_) * std::cos(yaw_);
    const float y = distancia_ * std::sin(pitch_);
    const float z = distancia_ * std::cos(pitch_) * std::sin(yaw_);
    const glm::vec3 eye = pos_avion + glm::vec3(x, y, z);

    // 4) La matriz de VISTA: glm::lookAt(eye, centro, up).
    //    Mira siempre al objetivo. Como el pitch está acotado, la dirección de
    //    vista y "up" nunca son colineales y lookAt queda bien definido.
    data_.view = glm::lookAt(eye, pos_avion, kUp);
}
