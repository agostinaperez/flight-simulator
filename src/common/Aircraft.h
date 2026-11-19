// Aircraft.h
// -----------------------------------------------------------------------------
// Módulo del MODELO DE LA AERONAVE: el avión como UNA composición de piezas.
// Es el dueño de las piezas (Mesh) y de las matrices que las ubican; el resto
// del programa NO sabe cuántas piezas tiene el avión ni cómo están armadas.
//
// La escena del práctico 3 tenía objetos independientes, cada uno con su propia
// matriz de modelo. El avión es un SOLO objeto hecho de varias piezas que se
// mueven JUNTAS: todas comparten la misma matriz de pose (por cuadro) y cada
// una tiene su matriz local (fija) que la ubica dentro del modelo.
//
// JERARQUÍA DE DOS NIVELES (básica del práctico):
//   M_mundo(pieza) = M_pose · M_local(pieza)
//     · M_local  : cómo está armada la pieza dentro del modelo (fija, init()).
//     · M_pose   : dónde y cómo está el avión en el mundo (por cuadro).
//   Basta con estos dos niveles porque el avión NO tiene piezas móviles: no hay
//   una cadena "el alerón depende del ala que depende del avión". Si algún día
//   una pieza necesitara moverse por su cuenta (ej. un alerón que deflecta),
//   haría falta un nivel más de jerarquía (una matriz por pieza móvil).
//
// DECISIONES DE DISEÑO (las "cuestiones a pensar" del práctico):
// ---------------------------------------------------------------
// [1] ¿Por qué separar init() (una vez) de update() (cada cuadro)?
//     → init() genera las mallas con los generadores de primitivas (calcular
//       vértices) y las sube a la VRAM. Eso es COSTOSO. update() solo rearma
//       una matriz 4x4 (la pose) con posición y ángulos: es baratísimo. Si se
//       regenerara todo en cada cuadro se volvería a generar y a subir la
//       geometría por frame para obtener casi siempre el MISMO resultado.
// [2] ¿Qué pasa si se llama collect() antes de update()?
//     → Se dibuja con la pose por defecto (la identidad): el avión aparece en
//       el origen del mundo, sin rotar. No rompe nada: es la pose "de fábrica".
// [3] ¿Y si se llama collect() antes de init()?
//     → piezas_ está vacía: collect() no agrega NINGÚN item y el loop de dibujo
//       no dibuja nada. Seguro, aunque sin sentido en un programa real.
// [4] ¿Por qué materiales_ además de piezas_ y locales_? (desviación del molde)
//     → El RenderItem de la guía lleva un material por pieza, pero el molde de
//       Aircraft no guardaba materiales. Sin ese vector nuevo no habría de
//       dónde sacar el material al hacer collect(). Es UN vector paralelo a
//       piezas_ (mismo largo, mismo orden de índice). En el Práctico 9 el
//       material reemplaza al color (ver Material.h): kd es el color difuso y
//       ks/ns el brillo especular.
// [5] ¿Cuántas mallas distintas y cuántas matrices locales?
//     → En init(): el ala y el empenaje horizontal se generan UNA vez como
//       malla y se reusan en dos piezas simétricas con matrices locales
//       distinas (una por lado). Por eso el conteo no coincide: son 5 mallas
//       distintas pero 7 piezas (7 matrices locales). La malla es un "molde"
//       de geometría; la pieza es una instancia de ese molde.
// [6] El día que el modelo venga de un .obj
//     → Cambia SOLO init() (cargar la malla desde archivo en vez de llamar a
//       primitives::*). update(), collect() y el main NO se tocan: la interfaz
//       (pose + lista de piezas listas) es la misma.
// [7] Copiar / mover
//     → Aircraft contiene std::vector<Mesh>, y Mesh prohíbe copiar (dueño
//       único de recursos de GPU). Por eso Aircraft tampoco es copiable (el
//       borrado se hereda del vector). No se declara mover: se usa como objeto
//       local en el main.
// -----------------------------------------------------------------------------
#pragma once                          // se incluye una sola vez por archivo

#include <vector>                     // std::vector: array que crece solo
#include <glm/glm.hpp>                // glm::mat4 / glm::vec3 / glm::vec4

#include "Mesh.h"                     // los Mesh propios (las piezas en GPU)
#include "Material.h"                 // el material de cada pieza (kd/ks/ns)
#include "RenderItem.h"               // el "contrato" de salida hacia el main

// Aircraft — el modelo de la aeronave (composición de piezas).
class Aircraft {
public:
    // init: arma las piezas y sus matrices locales, UNA SOLA VEZ.
    //   * Llama a los generadores de primitivas (primitives::*) y sube cada
    //     malla resultante a la GPU (un caga de <Mesh> en piezas_).
    //   * Calcula locales_ (UNA matriz por pieza): dónde queda cada pieza
    //     dentro del sistema de referencia del modelo. Son FIJAS mientras el
    //     avión no cambie de forma (por ahora no hay partes móviles).
    // Se puede volver a llamar: limpia lo anterior y rearma todo.
    void init();

    // update: recalcula SOLO la pose, cada cuadro, desde la posición y los
    // ángulos actuales del avión. No toca las piezas ni las matrices locales.
    //   pos     : dónde queda el PUNTO DE REFERENCIA del avión en el mundo.
    //   angulos : (cabeceo, guiñada, rolido) en RADIANES, rotaciones Aplicadas
    //             alrededor del punto de referencia (y no de un punto
    //             arbitrario del modelo). Ver README, sección pose.
    void update(glm::vec3 pos, glm::vec3 angulos);

    // set_pose: variante del PRÁCTICO 6 — recibe la orientación YA COMPUESTA
    // (una matriz 4x4 de rotación) en lugar de ángulos de Euler. La usa el FDM:
    // la actitud llega como ángulos (φ, θ, ψ) que to_world() + orientacion_vuelo()
    // convierten a UNA matriz (con su orden 3-2-1 ya resuelto). Pasar ángulos
    // acá sería incorrecto porque el orden de rotación del FDM (guiñada →
    // cabeceo → alabeo) no coincide con el orden de update(pos, angulos).
    //   pos          : la posición del PUNTO DE REFERENCIA (el CDG) en el mundo.
    //   orientacion  : matriz de rotación modelo -> mundo, ya calculada.
    //   escala_modelo: unidades de mundo por unidad del modelo normalizado.
    // La pose queda: M_pose = T(pos) · orientacion · S(escala) · T(-ref).
    // La escala se aplica entre la orientación y la traslación del punto de referencia.
    void set_pose(glm::vec3 pos, const glm::mat4& orientacion,
                  float escala_modelo = 1.0f);

    // collect: agrega al vector `items` UN RenderItem por pieza, con su
    // transformación YA compuesta (model = pose * local). Es el ÚNICO canal de
    // salida del modelo hacia el dibujo.
    void collect(std::vector<RenderItem>& items) const;

    // pieza_count: cuántas piezas hay (se reporta en la consola del main).
    std::size_t pieza_count() const;

private:
    // agregar_pieza: crea una pieza nueva al final de las tres listas.
    //   datos    : la malla recién generada (se la mueve), lista para subir a GPU.
    //   local    : la matriz local de ESTA pieza (fija desde init()).
    //   material : el material (kd/ks/ns) de ESTA pieza.
    void agregar_pieza(MeshData&& datos, const glm::mat4& local, const Material& material);

    // Las tres listas van PARALELAS: el elemento i de cada una es la misma pieza.
    std::vector<Mesh>      piezas_;    // las mallas en la GPU, UNA por pieza
    std::vector<glm::mat4> locales_;   // UNA matriz local por pieza (fijas)
    std::vector<Material>  materiales_;// UN material por pieza (paralelo a piezas_)
    glm::mat4 pose_ {1.0f};           // pose del avión (la actualiza update())
};