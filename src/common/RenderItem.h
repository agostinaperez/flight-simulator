// RenderItem.h
// -----------------------------------------------------------------------------
// RenderItem — la información mínima que un dibujador necesita para pintar UNA
// pieza: QUÉ malla, DÓNDE (matriz de modelo YA compuesta) y con QUÉ material.
//
// Es un struct de DATOS (solo campos, sin lógica): lo produce Aircraft::collect
// y lo consume el loop de dibujo del main. Mantiene el "contrato" entre el
// modelo (que sabe qué piezas tiene el avión) y el dibujo (que NO debe saber
// cuántas piezas hay ni cómo están armadas).
//
// DECISIONES DE DISEÑO:
//   * mesh es un PUNTERO a la malla que Aircraft posee (no una copia): la malla
//     no es propia de este struct, solo se PRESTA para dibujar. El puntero es
//     válido mientras Aircraft (el dueño) siga vivo; por eso el item se usa de
//     inmediato en el mismo cuadro.
//   * model ya viene COMPUESTA (pose * local): el que dibuja no necesita saber
//     cómo se armó el avión ni cuántos niveles de jerarquía tiene.
//   * PRÁCTICO 9: el campo `color` (vec4) fue REEMPLAZADO por `material`
//     (Material). Hasta el Práctico 8 cada pieza era un color plano; con la
//     iluminación el color pasa a ser solo el coeficiente difuso (material.kd),
//     y aparecen además el especular (ks) y el brillo (ns). Ver Material.h.
//   * PRÁCTICO 8: `texture` es un puntero OPCIONAL (nullptr = color plano).
//     Cuando una pieza lleva textura, el shader texturado usa esa textura como
//     kd en lugar del color. La textura es PRESTADA (el dueño es otro módulo).
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

#include <glm/glm.hpp>                // glm::mat4 (tipos de GLM)

#include "Material.h"                 // el material de la pieza (kd/ks/ns)

class Mesh;                           // declaración adelantada: para tener un
                                      // PUNTERO no hace falta el cuerpo de Mesh
class Texture;                        // igual: el puntero no necesita el cuerpo

// RenderItem — una pieza lista para dibujar, sin saber cómo se construyó.
struct RenderItem {
    const Mesh* mesh = nullptr;        // malla a dibujar (prestada, no propia)
    glm::mat4 model = glm::mat4(1.0f); // modelo -> mundo, YA compuesta
    Material material;                 // el material (kd = color difuso, ks, ns)
    const Texture* texture = nullptr;  // textura opcional (nullptr = color plano)
};