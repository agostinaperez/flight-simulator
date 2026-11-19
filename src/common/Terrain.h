// Terrain.h
// -----------------------------------------------------------------------------
// Módulo generador de la MALLA DE TERRENO (Práctico 7).
//
// El terreno es la primera malla del proyecto que NO sale de una primitiva a
// mano ni de un generador paramétrico simple: se genera por programa a partir
// de un HEIGHTMAP (una grilla de números: una altura por nodo). Acá conviven
// dos responsabilidades SEPARADAS a propósito:
//
//   1. La FUENTE DE ALTURAS (clase `Alturas`): responde "¿qué altura hay en el
//      nodo (i, j)?". Hoy se genera con ruido procedural (FastNoiseLite), pero
//      está DESACOPLADA de la geometría: cambiarla por una que lea una imagen en
//      escala de grises o un archivo de datos topográficos se reduce a cambiar
//      UNA clase (el bucle de la grilla no se toca).
//   2. El GENERADOR DE LA MALLA (función `grid`): traduce "N celdas por lado +
//      separación + alturas" a la tupla MeshData (vértices + índices), sin
//      saber DE DÓNDE salen las alturas.
//
// DECISIONES DE DISEÑO (las "cuestiones a pensar" del práctico):
// ---------------------------------------------------------------
// [1] Malla INDEXADA (el motivo de no listar 240.000 vértices).
//     → En una grilla cada vértice interior lo comparten 6 triángulos, y los 6
//       usan la MISMA tupla (misma posición, misma normal, mismas coordenadas).
//       A diferencia del cubo del Práctico 2 (donde una esquina pertenece a 3
//       caras de distinto color y por eso se duplicaba), acá la tupla es única
//       por nodo → se indexa. Para N = 200: 40.401 vértices indexados en lugar
//       de 240.000 sin indexar (5,94x menos).
// [2] UNA normal por vértice (no por cara).
//     → La normal se calcula por DIFERENCIAS CENTRADAS sobre la función de
//       alturas (suave): un único vector por nodo. Es la elección de una
//       superficie SUAVE, y es lo que permite compartir el vértice. El método
//       alternativo (promediar las normales de los triángulos que lo tocan) es
//       general pero más costoso y se reserva para mallas importadas (que no
//       tienen función de alturas). Ver README, sección 8.
// [3] Las dos listas (vértices e índices) salen del generador; el orden de los
//     índices fija el WINDING. Con {a,d,c} + {a,c,b} las normales (y las caras)
//     apuntan hacia ARRIBA (+Y), que es lo que exige el recorte de caras
//     traseras (GL_CULL_FACE). Ver README.
// [4] El bucle de VÉRTICES va `i <= N` (hay N+1 por lado) y el de CELDAS va
//     `i < N`. Confundirlos NO da error de compilación: da índices que apuntan
//     fuera del buffer y la malla se deforma en silencio. Está documentado para
//     poder diagnosticarlo (cuestiones a pensar del PDF).
// [5] La grilla se genera en el rectángulo [0, N·separación]²: el nodo (i, j)
//     queda en la posición (j·separación, altura, i·separación). NO se centra
//     en el origen acá: centrar es una decisión de la ESCENA (se hace con la
//     matriz de modelo). Dejarla "en la esquina" hace que la esquina (0,0) sea
//     el origen de las coordenadas de textura y que, en el Práctico 8, el
//     "suelo continuado" del fondo coincida EXACTO con la textura de la malla
//     (sin desfase). Ver README, sección 9.
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

#include "MeshData.h"                 // lo que produce grid(): vértices + índices
#include "FastNoiseLite.h"            // la fuente de ruido procedural (header-only)

namespace terreno {

// Alturas — la fuente de alturas del heightmap. Devuelve la altura YA ESCALADA
// en unidades de escena (lo que va a la coordenada Y del vértice).
class Alturas {
public:
    // semilla     : fija el ruido (misma semilla = mismo terreno en cada corrida).
    // frecuencia  : cuántos nodos abarca una característica grande (0.01 ≈ 100 nodos).
    // amplitud    : desnivel máximo PEDIDO, en unidades de escena. ATENCIÓN: al
    //               sumar octavas (FBm) la salida se normaliza y en la práctica
    //               se usa ~1/3 del rango nominal: pedir ±1.2 da unos ±0.4.
    Alturas(int semilla, float frecuencia, float amplitud);

    // operator(): la altura en el nodo (i, j), ya escalada. Es lo que llama el
    // bucle de la grilla como `altura(i, j)`.
    float operator()(unsigned i, unsigned j) const;

private:
    FastNoiseLite ruido_;   // el generador de ruido (Perlin + octavas FBm)
    float amplitud_;        // el desnivel máximo pedido (escala vertical)
};

// grid — genera la malla INDEXADA del terreno de N x N celdas.
//   N          : cantidad de celdas por lado (hay N+1 vértices por lado).
//   separacion : ancho de una celda (distancia entre vértices), en unidades de
//                escena. Fija el detalle del relieve (ver README).
//   alturas    : la fuente de alturas (devuelve la altura escalada del nodo).
// Devuelve un MeshData con (N+1)² vértices (posición + normal + coordenadas de
// textura) y 6·N² índices, listos para subir a la GPU.
MeshData grid(unsigned N, float separacion, const Alturas& alturas);

// height_at: altura interpolada en la misma superficie triangular que genera
// grid(). Las coordenadas row/column están expresadas en celdas y se acotan al
// borde de la grilla para que el contacto use la geometría visible.
float height_at(unsigned N, const Alturas& alturas, float row, float column);

} // namespace terreno
