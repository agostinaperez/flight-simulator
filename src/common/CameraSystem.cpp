// CameraSystem.cpp — implementación de las tres vistas de cámara.
// No toca OpenGL: solo matemática de matrices (glm). Por eso se puede probar
// fuera del loop de render (ver tests/).

#include "CameraSystem.h"

#include <glm/gtc/matrix_transform.hpp>  // glm::lookAt / perspective / radians
#include <cmath>                         // std::sin / std::cos
#include <algorithm>                     // std::min / std::max (acotes)
#include <iostream>                      // std::cout (reportes)

// -----------------------------------------------------------------------------
// Configuración de la cámara (todo en un namespace ANÓNIMO: solo se ve acá).
// -----------------------------------------------------------------------------
namespace {

// --- Vista ORBITAL -----------------------------------------------------------
// Límite del pitch: 0.9 * pi/2. Evita que la cámara quede EXACTAMENTE sobre el
// eje vertical, caso en el que la dirección de vista y el vector "up" quedan
// colineales y el producto vectorial de lookAt se anula (matriz indefinida).
constexpr float kMaxPitch = 0.9f * 1.57079632679f;  // ~81° (0.9 * pi/2)

// Acotes de la distancia al objetivo. Un mínimo > 0 para que la cámara no
// traspase al avión, y un máximo para no perderlo de vista al alejarse.
constexpr float kDistMin = 2.0f;
constexpr float kDistMax = 6000.0f;

// Vector "arriba" del mundo para la vista orbital. La escena usa Y hacia arriba.
const glm::vec3 kUpMundo(0.0f, 1.0f, 0.0f);

// --- Vista de PERSECUCIÓN ----------------------------------------------------
// La cámara se coloca detrás del avión (en contra del morro) y un poco por
// encima, todo en el sistema del propio avión. Los valores se eligen en metros,
// coherentes con la escala visual del modelo (8 m).
constexpr float kDistPersecucion = 30.0f;  // [m] cuánto atrás queda la cámara
constexpr float kAlturaPersecucion = 10.0f;// [m] cuánto por encima queda la cámara
constexpr float kMiraAdelante = 12.0f;     // [m] punto adelante del morro al que apunta

// --- Vista de CABINA ---------------------------------------------------------
// Ojo del piloto: un poco delante y por encima del punto de referencia (el CDG),
// para simular que se mira por encima del panel. El morro del modelo queda a
// unos 3.2 m por delante del CDG (0.4 * 8 m), así que sigue a la vista.
constexpr float kDelanteCabina = 1.0f;  // [m] delante del CDG
constexpr float kArribaCabina  = 1.0f;  // [m] por encima del CDG

// Vectores base del MODELO de la aeronave (ver Aircraft.cpp, sistema de
// referencia): +Z va de la nariz a la cola, por eso el morro apunta a -Z.
const glm::vec4 kMorroModelo(0.0f, 0.0f, -1.0f, 0.0f);  // "adelante" en el modelo
const glm::vec4 kArribaModelo(0.0f, 1.0f, 0.0f, 0.0f);  // "arriba" en el modelo

// Nombres de los modos, en el orden del enum CameraMode. Se usan para informar
// por consola y para la telemetría.
constexpr const char* kNombreModos[] = {"orbital", "persecucion", "cabina"};

} // fin del namespace anónimo (nada se exporta)

// -----------------------------------------------------------------------------
// Constructor: guarda el tamaño inicial y arma la proyección inicial.
// -----------------------------------------------------------------------------
CameraSystem::CameraSystem(int width, int height) {
    set_viewport(width, height);   // calcula aspect + proyección inicial
    std::cout << "[CameraSystem] vista inicial: " << mode_name() << '\n';
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
// update: elige el cálculo de VISTA según el modo activo.
// -----------------------------------------------------------------------------
void CameraSystem::update(glm::vec3 pos_avion, const glm::mat4& orientacion_avion,
                          const CameraCommand& cmd) {
    switch (modo_) {
        case CameraMode::Orbital:
            update_orbital(pos_avion, cmd);
            break;
        case CameraMode::Chase:
            update_chase(pos_avion, orientacion_avion);
            break;
        case CameraMode::Cockpit:
            update_cockpit(pos_avion, orientacion_avion);
            break;
    }
}

// -----------------------------------------------------------------------------
// cycle_mode: avanza al modo siguiente e informa por consola.
// -----------------------------------------------------------------------------
void CameraSystem::cycle_mode() {
    // Avanzo al modo siguiente y vuelvo al primero al pasar el último (anillo).
    switch (modo_) {
        case CameraMode::Orbital:  modo_ = CameraMode::Chase;    break;
        case CameraMode::Chase:    modo_ = CameraMode::Cockpit;  break;
        case CameraMode::Cockpit:  modo_ = CameraMode::Orbital;  break;
    }
    std::cout << "[CameraSystem] vista: " << mode_name() << '\n';
}

// -----------------------------------------------------------------------------
// mode_name: nombre legible del modo activo.
// -----------------------------------------------------------------------------
const char* CameraSystem::mode_name() const {
    return kNombreModos[static_cast<int>(modo_)];
}

// -----------------------------------------------------------------------------
// update_orbital: acumula los deltas del usuario y recalcula la matriz de VISTA.
// -----------------------------------------------------------------------------
void CameraSystem::update_orbital(glm::vec3 pos_avion, const CameraCommand& cmd) {
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
    data_.view = glm::lookAt(eye, pos_avion, kUpMundo);
}

// -----------------------------------------------------------------------------
// update_chase: cámara detrás y arriba del avión, montada sobre su orientación.
// -----------------------------------------------------------------------------
void CameraSystem::update_chase(glm::vec3 pos_avion, const glm::mat4& orientacion) {
    // 1) Saco de la matriz de orientación los dos vectores que necesito:
    //    el MORRO (adelante) y el ARRIBA del avión, ya en coordenadas de mundo.
    //    La matriz transforma del modelo al mundo, así que basta con aplicarla
    //    a los vectores base del modelo. El componente w = 0 descarta la
    //    traslación (que en orientacion es cero igual).
    const glm::vec3 morro = glm::normalize(
        glm::vec3(orientacion * kMorroModelo));
    const glm::vec3 arriba = glm::normalize(
        glm::vec3(orientacion * kArribaModelo));

    // 2) La cámara queda detrás (en contra del morro) y por encima del avión.
    const glm::vec3 eye = pos_avion
        - morro * kDistPersecucion
        + arriba * kAlturaPersecucion;

    // 3) Apunta a un punto algo más adelante del morro: el avión queda encuadrado
    //    un poco por debajo del centro de la pantalla, dejando ver hacia dónde va.
    const glm::vec3 objetivo = pos_avion + morro * kMiraAdelante;

    // 4) lookAt con el "up" del propio avión: la vista SE INCLINA con la
    //    aeronave (acompaña bancos y loop). forward y up del cuerpo son
    //    ortogonales, así que la dirección de vista (forward·const1 - up·const2)
    //    nunca queda paralela a up: lookAt siempre está bien definido (decisión [4]).
    data_.view = glm::lookAt(eye, objetivo, arriba);
}

// -----------------------------------------------------------------------------
// update_cockpit: cámara en el ojo del piloto, mirando al morro.
// -----------------------------------------------------------------------------
void CameraSystem::update_cockpit(glm::vec3 pos_avion, const glm::mat4& orientacion) {
    // 1) Mismos dos vectores que en la persecución: morro y arriba en el mundo.
    const glm::vec3 morro = glm::normalize(
        glm::vec3(orientacion * kMorroModelo));
    const glm::vec3 arriba = glm::normalize(
        glm::vec3(orientacion * kArribaModelo));

    // 2) El ojo del piloto: un poco delante y por encima del CDG (ver constantes).
    const glm::vec3 eye = pos_avion
        + morro * kDelanteCabina
        + arriba * kArribaCabina;

    // 3) La vista mira al morro: eye + morro. El "up" es el del avión, de modo
    //    que la cámara queda SOLIDARIA con la actitud (la misma del FDM).
    data_.view = glm::lookAt(eye, eye + morro, arriba);
}
