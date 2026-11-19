// avion_lit.vs — Vertex shader de la aeronave ILUMINADA (Práctico 9).
//
// Como el terreno_lit.vs, calcula posición y normal en coordenadas de MUNDO. La
// diferencia importante es la MATRIZ NORMAL (uMatNormal): las piezas del avión
// tienen escalado NO uniforme (las placas se generan con cube(escala)), y para
// transformar una normal NO se puede usar la matriz de modelo directamente (deja
// de ser perpendicular a la superficie). La matriz correcta es G = (M^-1)^T, que
// se calcula UNA vez por pieza en el CPU y llega como uniform mat3.

#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;                       // modelo -> mundo
uniform mat4 uView;                        // mundo -> cámara
uniform mat4 uProjection;                  // cámara -> clip
uniform mat3 uMatNormal;                   // (M^-1)^T: transforma normales al mundo

out vec3 vPosMundo;
out vec3 vNormalMundo;

void main()
{
    vec4 posMundo = uModel * vec4(aPos, 1.0);
    vPosMundo     = posMundo.xyz;
    vNormalMundo  = uMatNormal * aNormal;  // la normal, con la matriz normal
    gl_Position   = uProjection * uView * posMundo;
}
