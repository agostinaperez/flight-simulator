// MeshData.h

// Definición de los DATOS de la malla del lado de la CPU.
// Esta parte vive ANTES que cualquier módulo de OpenGL: describe cómo se ven los datos en la memoria RAM, sin tocar la GPU
// Los vértices van INTERLAZADOS ("interleaved"): dentro de cada Vertex, los
// tres atributos (posición, normal y textura) están uno al lado del otro, en el
// mismo bloque. Cada vértice ocupa 8 floats (pos = 3 + normal = 3 + uv = 2) =
// 8 * 4 bytes = 32 bytes.
// Ese bloque entrelazado es lo que se sube a la GPU como UN SOLO VBO.
//
// Los módulos que usan esto:
//   * Mesh        : lee `Vertex` para saber el "formato" (offsets de atributos).
//   * Primitives  : produce y rellena estos structs (generador de mallas).

#pragma once                         

#include <glm/glm.hpp>                // glm::vec3 / glm::vec2: vectores de GLM
#include <vector>                     // std::vector: array que crece solo (RAM)
#include <cstddef>                    // offsetof (no se usa acá, pero es el header
                                      // natural de estos datos; lo usan los demás)


// Vertex — atributos de un vértice.
// El orden de los campos ES el orden en que se guardan en memoria.
// Cambiar este struct (ej. agregar un atributo) cambia el formato del buffer;
// por eso los módulos que lo consumen calculan los desplazamientos con
// offsetof() y no los escriben a mano.
//
// PRÁCTICO 3 — esta es la tupla NUEVA:
//   * `position`  : dónde está el vértice (igual que antes, en glm::vec3).
//   * `normal`    : la dirección perpendicular a la superficie en ese vértice.
//                   Hoy se calcula y se guarda pero NADIE la consume todavía
//                   (la iluminación llega en la Unidad IX). Sirve para
//                   verificar que esté bien calculada pintándola como color.
//   * `tex_coords`: coordenadas de textura (u,v), el lugar del vértice dentro
//                   de la imagen que se mapeará al objeto. Hoy se calculan y se
//                   guardan pero tampoco se usan aún (texturas más adelante).
//
//   El COLOR ya NO está acá: no es un atributo del vértice, es del OBJETO.
//   Los colores de antes cambiaban en cada vértice/arista; ahora una pieza
//   tiene UN color para todos sus vértices, que llega al shader por UNIFORM.

struct Vertex {
    glm::vec3 position;               // posición: coordenadas X, Y, Z del vértice
    glm::vec3 normal;                 // normal: dirección perpendicular (afuera)
    glm::vec2 tex_coords;             // textura: (u, v) dentro de la imagen
};                                    // fin de la tupla (32 bytes en memoria)


// los datos de una malla "sin procesar": todo lo que se necesita para subirla a la GPU después.
struct MeshData {
    // Lista de vértices (uno por tupla). En una malla INDEXADA pueden haber vertices de = posicion pero != color, x ende son distintos
    std::vector<Vertex> vertices;

    // Lista de ÍNDICES: cada 3 números suman un triángulo que "apunta" a los
    // vértices de `vertices`. Los datos de los vértices se escriben una sola vez y los
    // triángulos los referencian: es el trabajo de 36 índices → glDrawElements.
    std::vector<unsigned int> indices;
};                                    