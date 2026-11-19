// CameraData.h
// -----------------------------------------------------------------------------
// CameraData — el PAR de matrices que completan el proceso de proyección por
// cuadro: la de VISTA (V) y la de PROYECCIÓN (P). Es un struct de DATOS puro
// (solo campos, sin lógica).
//
// Es lo ÚNICO que el resto del programa necesita saber de la cámara: por cuadro
// pide este par y lo manda a los uniforms uView y uProjection del shader. No le
// importa si la cámara es orbital, de cabina o fija: esa decisión queda DENTRO
// de CameraSystem y acá solo se expone el resultado.
//
// Por qué separar V de P (en vez de juntarlas en una sola "uAjuste"):
//   * La VISTA describe dónde y cómo está parada la cámara (glm::lookAt).
//   * La PROYECCIÓN describe el tipo de lente (glm::perspective: fov, aspect,
//     near, far). Cambia con el tamaño de la ventana (aspect), por eso se
//     recalcula en set_viewport(), pero la vista NO depende de la ventana.
//   Mantenerlas separadas es lo que permite, más adelante, cambiar el tipo de
//   proyección (ortográfica) o el tipo de vista (cabina) sin tocar el resto.
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

#include <glm/glm.hpp>                // glm::mat4: matrices 4x4 de GLM

// CameraData — los datos para construir la proyección (un par por cuadro).
struct CameraData {
    glm::mat4 view       = glm::mat4(1.0f);  // V: mundo -> espacio de cámara
    glm::mat4 projection = glm::mat4(1.0f);  // P: cámara -> espacio de clip
};
