// Módulo GENERADOR de mallas de figuras primitivas. Traduce una figura geométrica (definida por parámetros) a la tupla MeshData.

// TODAS las figuras quedan CENTRADAS EN EL ORIGEN, por lo q la posición real en la escena la decide la MATRIZ DE MODELO (translate/
// rotate/scale desde el main), y así el módulo es predecible.
#pragma once                         

#include "MeshData.h" 
#include <stdexcept>                  // std::invalid_argument (guard de gajos)

// namespace primitives. NO es una clase x ende no hace falta instanciarla ni constructores. Son funciones q viven acá
namespace primitives {

// cube: genera la malla INDEXADA de un cubo (24 vértices, 36 índices).
//   scale_x, scale_y, scale_z : tamaño total del cubo en cada eje (lado, alto, profundidad). Con los valores por defecto (1,1,1)
//   da un cubo de LADO UNITARIO (lado 1), centrado en el origen (cada vértice a ±0.5).
// Variando los tres valores se pueden hacer otras figuras de caras paralelas(ej. una placa delgada: cube(2, 0.1, 2) — útil para las alas del avión).
// Cada cara tiene su NORMAL constante (los 4 vértices la comparten) y sus coordenadas de textura (0,0)(1,0)(1,1)(0,1). Se mantienen 24 vértices porque
// la normal cambia en las aristas (una esquina es de 3 caras con 3 normales).
MeshData cube(float scale_x = 1.0f,   // tamaño en X (ancho)
              float scale_y = 1.0f,   // tamaño en Y (alto)
              float scale_z = 1.0f);  // tamaño en Z (profundidad)


// cylinder: genera la malla INDEXADA de la SUPERFICIE LATERAL de un cilindro,
// centrado en el origen y con su eje sobre Y (base abajo en -largo/2, base arriba en +largo/2).
//   radio, largo, gajos (cantidad de segmentos en q se divide la vuelta, x discretización. A + gajos, + redondo)

// Por anillo se generan gajos+1 vértices (a lo largo del loop se usa i <= gajos). En la COSTURA (donde el último gajo toca al primero) la
// posición y la normal coinciden pero la coordenada de textura vale 0 y 1 a la vez: un atributo difiere => el vértice se duplica
// La normal es RADIAL y varía de forma CONTINUA entre gajos vecinos: por eso el borde de dos gajos sí se puede compartir
// Lanza std::invalid_argument si gajos < 3 (no se puede cerrar la vuelta).
MeshData cylinder(float radio, float largo, unsigned int gajos); 


// cone: genera la malla INDEXADA de la SUPERFICIE LATERAL de un cono, centrado en el origen y con el eje sobre Y (base en -H/2, ápice en
// +H/2, donde la altura H = radio / tan(α/2) se deduce del semiángulo).
// radio de la BASE, angulo_conicidad : ángulo α (en GRADOS) que forma la superficie con su eje: α chico = cono largo y finito; α grande = cono bajo y ancho.
// gajos (cantidad de segmentos en que se divide la vuelta).

// Normales:
// Lateral: NO es radial como la del cilindro; está inclinada hacia arriba por el semiángulo α/2: n = (cosθ·cos(α/2), sin(α/2), sinθ·cos(α/2)).
// Ápice: criterio FIJADO: su normal es axial (0,1,0) y NO comparte el vértice con el lateral (un atributo difiere => se duplica).
// Lanza std::invalid_argument si gajos < 3.
MeshData cone(float radio, float angulo_conicidad, unsigned int gajos);

}