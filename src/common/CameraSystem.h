// CameraSystem.h
// -----------------------------------------------------------------------------
// CameraSystem — el ADMINISTRADOR del estado de la cámara orbital. Es un módulo
// de C++ puro: NO lee el mouse, NO dibuja y NO usa OpenGL. Solo:
//   * guarda el estado de la órbita (yaw, pitch, distancia),
//   * recibe los deltas del usuario (CameraCommand) y los acumula,
//   * calcula la matriz de VISTA con glm::lookAt, y
//   * calcula/actualiza la matriz de PROYECCIÓN con glm::perspective.
//
// Por qué conviene agrupar esto en un módulo (en vez de dejarlo en el main):
//   * El resto del programa no necesita saber qué TIPO de cámara es (orbital,
//     de cabina, fija): solo pide, por cuadro, el par de matrices (CameraData)
//     para completar el proceso de proyección (vista V + proyección P).
//   * Separar el estado de la cámara de la escena permite agregar, más adelante,
//     otros tipos de vista sin ajustar el resto del programa.
//
// DECISIONES DE DISEÑO (las "cuestiones a pensar" del práctico):
// ---------------------------------------------------------------
// [1] ¿Por qué update() recibe los ángulos de la aeronave si no los usa?
//     → Para la cámara ORBITAL no hacen falta: siempre mira al objetivo. Pero
//       el parámetro queda "reservado" para un tipo de vista que SÍ los
//       necesite (ej. una vista de cabina, que se orienta con la actitud del
//       avión). Que la interfaz sea la misma para todas las vistas es lo que
//       permite cambiarlas sin tocar el main. Por ahora se recibe y se ignora,
//       documentado acá.
// [2] ¿Dónde se evita la división por cero de aspect (ventana minimizada)?
//     → En set_viewport(): si el alto llega a 0 (o es negativo, que pasa al
//       minimizar), se usa 1 como alto para el cálculo. La proyección queda
//       "deforme" pero definida; en cuanto se restaura la ventana, se recalcula
//       bien. Ver el comentario dentro de set_viewport().
// [3] La proyección en perspectiva usa fovy en RADIANES. Pasar grados no da
//     error de compilación: da pantalla negra (el ángulo queda 45x más grande
//     de lo esperado y todo cae fuera del frustum). Por eso se escribe
//     glm::radians(45.0f).
// [4] El pitch se acota a +-0.9·(π/2). En ±π/2 la cámara quedaría JUSTO sobre
//     el eje vertical, con la dirección de vista y el vector "up" colineales:
//     el producto vectorial interno de lookAt se hace cero y la matriz queda
//     indefinida (salto de imagen). Acotar antes evita el caso degenerado.
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

#include <glm/glm.hpp>                // glm::mat4 / glm::vec3

#include "CameraData.h"               // el par view/projection que entrega data()
#include "CameraCommand.h"            // los deltas que recibe update()

// CameraSystem — administra el estado de la cámara orbital.
class CameraSystem {
public:
    // Constructor: guarda el tamaño inicial de la ventana y arma la proyección
    // inicial (aspect = width/height). Sin estado de órbita aún (valores por
    // defecto de yaw/pitch/distancia).
    CameraSystem(int width, int height);

    // update: recalcula, por cuadro, la matriz de VISTA desde el estado de la
    // órbita y los deltas recibidos.
    //   pos_avion     : dónde está el objetivo (punto al que la cámara mira).
    //   angulos_avion : la actitud del avión. NO se usa para la cámara orbital
    //                   (ver decisión [1]); se recibe para compatibilidad con
    //                   vistas futuras.
    //   cmd           : los deltas del usuario (yaw/pitch/dist) de este cuadro.
    void update(glm::vec3 pos_avion, glm::vec3 angulos_avion, const CameraCommand& cmd);

    // set_viewport: al redimensionar la ventana, recalcula la PROYECCIÓN con el
    // nuevo aspect (el fov, near y far no cambian; solo el aspect).
    void set_viewport(int width, int height);

    // data: el par de matrices (view + projection) para mandar al shader.
    const CameraData& data() const { return data_; }

private:
    // Estado de la órbita (los tres números que describen a la cámara orbital).
    float yaw_       = 0.0f;   // [rad] longitud sobre la órbita
    float pitch_     = 0.0f;   // [rad] latitud (acotada, ver decisión [4])
    float distancia_ = 35.0f;  // [m] distancia inicial para encuadrar el avión y el terreno

    // Parámetros de la proyección en perspectiva (fijos salvo aspect).
    float fovy_ = glm::radians(45.0f);   // ángulo de visión vertical (radianes)
    float near_ = 0.1f;                  // plano de recorte cercano
    float far_  = 10000.0f;              // el mundo del FDM está expresado en metros

    CameraData data_{};                  // el par view/projection ya calculado
};
