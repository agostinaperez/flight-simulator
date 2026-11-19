// background.vs — Vertex shader del fondo (cielo + suelo continuado), Práctico 8.
//
// El fondo es un CUBO centrado en el ojo (la cámara) al que se le aplica una
// textura cubemap. Este shader:
//   * NO usa la matriz de modelo: el cubo está en el origen de su propio sistema.
//   * Usa una matriz de VISTA SIN TRASLACIÓN (solo la rotación): así el cubo
//     "sigue" al observador, que no tiene forma de medir su distancia (sin
//     parallax => lo percibe como infinitamente lejano).
//   * Pasa la POSICIÓN del vértice como vDireccion: como el cubo está centrado
//     en el origen, la posición de cada vértice ES la dirección desde el centro
//     con la que se muestrea el cubemap. No hace falta que sea unitaria.

#version 460 core

layout (location = 0) in vec3 aPos;        // posición del vértice del cubo

uniform mat4 uProjection;                  // la proyección normal (perspectiva)
uniform mat4 uView;                        // la vista SIN traslación (solo rotación)

out vec3 vDireccion;                       // la dirección desde el ojo a este vértice

void main()
{
    vDireccion  = aPos;                    // desde el centro, cada vértice es una dirección
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
