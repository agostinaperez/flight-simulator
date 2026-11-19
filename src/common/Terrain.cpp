// Terrain.cpp — implementación del generador de malla de terreno.
// Este archivo NO toca OpenGL: solo fabrica números (Vertex + índices) en RAM,
// igual que Primitives.cpp. La subida a la GPU la hace el módulo Mesh.

#include "Terrain.h"

#include <glm/glm.hpp>                // glm::vec2 / glm::vec3 / glm::normalize
#include <cmath>                       // std::isfinite
#include <stdexcept>                  // std::invalid_argument (guards de parámetros)
#include <vector>                     // std::vector (vía MeshData.h)
#include <algorithm>                  // std::min / std::max (acotar bordes de la normal)

namespace terreno {

// -----------------------------------------------------------------------------
// Alturas: constructor. Configura FastNoiseLite para un relieve de tipo
// "colinas" (Perlin con FBm de 5 octavas).
// -----------------------------------------------------------------------------
Alturas::Alturas(int semilla, float frecuencia, float amplitud)
    : amplitud_(amplitud) {
    ruido_.SetSeed(semilla);                          // misma semilla = mismo terreno
    ruido_.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    ruido_.SetFractalType(FastNoiseLite::FractalType_FBm);  // suma de octavas
    ruido_.SetFractalOctaves(5);                      // detalle fino sobre el relieve
    ruido_.SetFrequency(frecuencia);
}

// -----------------------------------------------------------------------------
// operator(): la altura del nodo (i, j). GetNoise devuelve ~[-1, 1]; la
// amplitud lo lleva a la escala de la escena. Se muestrea con (x = j, y = i)
// para que coincida con la posición (x = j·separación, z = i·separación) del
// bucle de la grilla.
// -----------------------------------------------------------------------------
float Alturas::operator()(unsigned i, unsigned j) const {
    return amplitud_ * ruido_.GetNoise(static_cast<float>(j),
                                       static_cast<float>(i));
}

// -----------------------------------------------------------------------------
// grid: genera la grilla regular de (N+1)² vértices y los 6·N² índices.
// -----------------------------------------------------------------------------
MeshData grid(unsigned N, float separacion, const Alturas& alturas) {
    // Guards "no romper en silencio": una grilla degenerada no es un terreno.
    if (N < 1U) {
        throw std::invalid_argument("terreno::grid: N debe ser >= 1");
    }
    if (separacion <= 0.0f) {
        throw std::invalid_argument("terreno::grid: separacion debe ser > 0");
    }

    MeshData m;                         // la malla que voy a ir llenando

    // PARTE 1 — Los vértices. OJO con el límite: i <= N porque hay N+1 vértices
    // por lado (N celdas delimitan N+1 nodos). Cada vértice lleva:
    //   * position   = (j·paso, altura(i,j), i·paso): la grilla es regular en x,z
    //                  y la altura la aporta el heightmap.
    //   * tex_coords = (j/N, i/N) en [0,1]: el lugar del nodo dentro de la grilla.
    //                  En el Práctico 8 se multiplican por el número de teselas.
    //   * normal     = la de una superficie suave (diferencias centradas).
    for (unsigned i = 0; i <= N; ++i) {
        for (unsigned j = 0; j <= N; ++j) {
            Vertex v;
            v.position   = glm::vec3(j * separacion, alturas(i, j), i * separacion);
            v.tex_coords = glm::vec2(static_cast<float>(j) / static_cast<float>(N),
                                     static_cast<float>(i) / static_cast<float>(N));

            // PARTE 2 — La normal por diferencias centradas (ver README, sección 8).
            // Para y = f(x, z), la normal hacia arriba es n = (-df/dx, 1, -df/dz).
            // Con datos discretos las derivadas se estiman por diferencias
            // centradas sobre los nodos vecinos; el factor 2·d (la separación) que
            // aparece en el componente Y compensa exactamente el denominador de la
            // derivada central, de modo que el vector (antes de normalizar) queda
            // proporcional a la normal real. La normalización final lo hace unitario.
            //
            // En el BORDE no hay cuatro vecinos: se acotan los índices a [0, N]
            // (diferencia de un solo lado). Lo único prohibido es dejar la normal
            // en cero: normalizar un vector nulo no está definido.
            const unsigned im = (i > 0) ? (i - 1) : 0;         // vecino i-1 (acotado)
            const unsigned ip = (i < N) ? (i + 1) : N;         // vecino i+1
            const unsigned jm = (j > 0) ? (j - 1) : 0;         // vecino j-1
            const unsigned jp = (j < N) ? (j + 1) : N;         // vecino j+1

            // -df/dx usa los vecinos en j (x = j·separación); -df/dz usa los
            // vecinos en i (z = i·separación).
            const float dfdx = alturas(i, jm) - alturas(i, jp);   // = -2d·df/dx
            const float dfdz = alturas(im, j) - alturas(ip, j);   // = -2d·df/dz

            v.normal = glm::normalize(glm::vec3(dfdx, 2.0f * separacion, dfdz));
            m.vertices.push_back(v);
        }
    }

    // PARTE 1 (índices) — Acá el límite es < N porque se recorren CELDAS.
    // `fila` es el ancho en VÉRTICES (no en celdas): N+1 por fila.
    const unsigned fila = N + 1U;
    auto idx = [fila](unsigned i, unsigned j) { return i * fila + j; };

    for (unsigned i = 0; i < N; ++i) {
        for (unsigned j = 0; j < N; ++j) {
            // Las 4 esquinas de la celda (i, j), en términos de (fila, columna):
            //   a = (i, j)     b = (i, j+1)     (fila i)
            //   d = (i+1, j)   c = (i+1, j+1)   (fila i+1)
            const unsigned a = idx(i,     j);
            const unsigned b = idx(i,     j + 1U);
            const unsigned c = idx(i + 1U, j + 1U);
            const unsigned d = idx(i + 1U, j);

            // La celda se parte en DOS triángulos que comparten la diagonal a-c.
            // El orden {a,d,c} y {a,c,b} deja la normal de cada triángulo
            // apuntando hacia ARRIBA (+Y): es el winding que necesita el recorte
            // de caras traseras para que el terreno se vea desde arriba y NO se
            // vea desde abajo. (Verificación hecha a mano en el README, sección 8.)
            m.indices.insert(m.indices.end(), {a, d, c});
            m.indices.insert(m.indices.end(), {a, c, b});
        }
    }

    return m;                          // (N+1)² vértices + 6·N² índices
}

float height_at(unsigned N, const Alturas& alturas, float row, float column) {
    if (N < 1U || !std::isfinite(row) || !std::isfinite(column)) {
        throw std::invalid_argument(
            "terreno::height_at: N debe ser >= 1 y las coordenadas finitas");
    }

    const float maximo = static_cast<float>(N);
    const float fila = std::clamp(row, 0.0f, maximo);
    const float columna = std::clamp(column, 0.0f, maximo);
    const unsigned i = std::min(static_cast<unsigned>(fila), N - 1U);
    const unsigned j = std::min(static_cast<unsigned>(columna), N - 1U);
    const float fraccion_fila = fila - static_cast<float>(i);
    const float fraccion_columna = columna - static_cast<float>(j);

    const float a = alturas(i, j);
    const float b = alturas(i, j + 1U);
    const float c = alturas(i + 1U, j + 1U);
    const float d = alturas(i + 1U, j);

    // grid() divide la celda por la diagonal a-c; se usa la misma interpolación
    // por triángulos para que el suelo físico coincida con el que se dibuja.
    if (fraccion_columna <= fraccion_fila) {
        return a * (1.0f - fraccion_fila)
             + d * (fraccion_fila - fraccion_columna)
             + c * fraccion_columna;
    }
    return a * (1.0f - fraccion_columna)
         + b * (fraccion_columna - fraccion_fila)
         + c * fraccion_fila;
}

} // namespace terreno
