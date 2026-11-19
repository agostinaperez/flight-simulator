// Material.h
// -----------------------------------------------------------------------------
// Material — los coeficientes del modelo de iluminación de Phong para UNA
// superficie (una pieza de la aeronave, el terreno, etc.). Es un struct de
// DATOS puro (solo campos, sin lógica).
//
// Por qué existe este struct (y por qué reemplaza al `color` del RenderItem):
//   * Hasta el Práctico 8 cada pieza se pintaba con UN color plano. En el
//     Práctico 9 llega la iluminación: el color final de cada fragmento deja de
//     ser "el color de la pieza" y pasa a depender de la normal, la luz y el
//     observador. Lo que antes era "el color" ahora es solo el coeficiente
//     DIFUSO (kd): cuánto de cada canal de luz la superficie refleja de forma
//     difusa (pareja en todas direcciones).
//   * kd, ks y ns son los parámetros del modelo de Phong. Son un dato de CADA
//     objeto (cada material es distinto), no de la escena ni de la luz.
//
// Los nombres coinciden EXACTAMENTE con el struct `Material` del código GLSL
// común (assets/shaders/iluminacion.glsl): eso permite pasar este struct a la
// función de iluminación por uniform como `uniform Material uMaterial;` y
// direccionar cada campo con glGetUniformLocation("uMaterial.kd"), etc.
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

#include <glm/glm.hpp>                // glm::vec3: los coeficientes de color

// Material — coeficientes del modelo de Phong de una superficie.
struct Material {
    glm::vec3 kd = glm::vec3(1.0f);    // coeficiente difuso (y ambiente: ka = kd).
                                       //   Es "el color" que antes llevaba el
                                       //   RenderItem: qué color de luz refleja la
                                       //   superficie de forma pareja.
    glm::vec3 ks = glm::vec3(0.0f);    // coeficiente especular: qué fracción de la
                                       //   luz se refleja como brillo. 0 = sin brillo
                                       //   (superficies mate como el terreno).
    float ns = 1.0f;                   // exponente especular (el "brillo"): qué tan
                                       //   concentrado es el reflejo. Más grande =
                                       //   brillo más puntual (superficie pulida).
};
