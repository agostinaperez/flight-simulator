// Primitives.h
//
// Módulo GENERADOR de mallas de figuras primitivas. Traduce una figura
// geométrica (definida por parámetros) a la tupla MeshData.
// DECISIONES DE DISEÑO (las "cuestiones a pensar" del práctico):
//     → FUNCIÓN LIBRE dentro de un namespace. NADA posee a este objeto. Un generador no posee recursos de GPU ni estado: toma
//       parámetros y devuelve un MeshData (objeto por valor). Si no posee nada no hace falta destructor, ni constructor, ni clase
//
//     → El COLOR ya no vive acá: en el Práctico 3 el color dejó de ser un
//       atributo del vértice y pasó a ser del OBJETO (se setea por uniform).
//
//     → TODAS las figuras quedan CENTRADAS EN EL ORIGEN (el cubo en [-L/2,+L/2],
//       cilindro y cono con su mitad a cada lado del plano XY). Ventajas: la
//       posición real en la escena la decide la MATRIZ DE MODELO (translate/
//       rotate/scale desde el main), y así el módulo es predecible.
//
//     → El módulo crece AGREGANDO funciones (así nacieron cylinder() y cone()),
//       no cambiando las existentes. Si alguna vez hiciera falta un paso común
//       (ej. generar las normales), se extrae una función auxiliar interna.
//
//     → Errores "no en silencio": si un parámetro invalida la figura (ej.
//       menos de 3 gajos), se lanza std::invalid_argument con un mensaje
//       descriptivo. Un generador que acepta basura y devuelve una malla
//       rota sería un bug silencioso.
#pragma once                         

#include "MeshData.h" 
#include <stdexcept>                  // std::invalid_argument (guard de gajos)

// namespace primitives — todos los generadores de figuras viven acá
namespace primitives {

// cube: genera la malla INDEXADA de un cubo (24 vértices, 36 índices).
//   scale_x, scale_y, scale_z : tamaño total del cubo en cada eje (lado, alto,
//                               profundidad). Con los valores por defecto (1,1,1)
//                               da un cubo de LADO UNITARIO (lado 1), centrado en
//                               el origen (cada vértice a ±0.5).
// Variando los tres valores se pueden hacer otras figuras de caras paralelas
// (ej. una placa delgada: cube(2, 0.1, 2) — útil para las alas del avión).
// Cada cara tiene su NORMAL constante (los 4 vértices la comparten) y sus
// coordenadas de textura (0,0)(1,0)(1,1)(0,1). Se mantienen 24 vértices porque
// la normal cambia en las aristas (una esquina es de 3 caras con 3 normales).
MeshData cube(float scale_x = 1.0f,   // tamaño en X (ancho)
              float scale_y = 1.0f,   // tamaño en Y (alto)
              float scale_z = 1.0f);  // tamaño en Z (profundidad)


// cylinder: genera la malla INDEXADA de la SUPERFICIE LATERAL de un cilindro,
// centrado en el origen y con su eje sobre Y (base abajo en -largo/2, base
// arriba en +largo/2).
//   radio    : distancia del borde al eje (la "anchura" del cilindro).
//   largo    : cuánto mide el cilindro a lo largo de su eje Y.
//   gajos    : cantidad de "gajos" (segmentos) en que se divide la vuelta
//              (discretización polar/diametral). Cuantos más, más redondo.
//
// Modelado paramétrico del PDF:
//   * Por anillo se generan gajos+1 vértices (a lo largo del loop se usa
//     i <= gajos). En la COSTURA (donde el último gajo toca al primero) la
//     posición y la normal coinciden pero la coordenada de textura vale 0 y 1
//     a la vez: un atributo difiere => el vértice se duplica (regla del cubo).
//   * La normal es RADIAL y varía de forma CONTINUA entre gajos vecinos: por
//     eso el borde de dos gajos sí se puede compartir (18 vértices para 8
//     gajos, en vez de los 32 de la tupla vieja con color por gajo).
//   * Lanza std::invalid_argument si gajos < 3 (no se puede cerrar la vuelta).
MeshData cylinder(float radio,          // cuánto se abre la base (distancia al eje)
                  float largo,          // longitud a lo largo del eje Y
                  unsigned int gajos);  // cantidad de segmentos en la vuelta


// cone: genera la malla INDEXADA de la SUPERFICIE LATERAL de un cono (sin
// tapa), centrado en el origen y con el eje sobre Y (base en -H/2, ápice en
// +H/2, donde la altura H = radio / tan(α/2) se deduce del semiángulo).
//   radio   : radio de la BASE (el anillo inferior).
//   angulo_conicidad : ángulo α (en GRADOS) que forma la superficie con su eje:
//                      cómo "puntiagudo" es. α chico = cono largo y finito;
//                      α grande = cono bajo y ancho.
//   gajos   : cantidad de "gajos" (segmentos) en que se divide la vuelta.
//
// Normales del PDF:
//   * Lateral  : NO es radial como la del cilindro; está inclinada hacia arriba
//                por el semiángulo α/2: n = (cosθ·cos(α/2), sin(α/2), sinθ·cos(α/2)).
//   * Ápice    : criterio FIJADO: su normal es axial (0,1,0) y NO comparte el
//                vértice con el lateral (un atributo difiere => se duplica).
//   * Lanza std::invalid_argument si gajos < 3.
MeshData cone(float radio,             // radio de la base (anillo inferior)
              float angulo_conicidad,  // ángulo de conicidad α en grados
              unsigned int gajos);     // cantidad de segmentos en la vuelta

}