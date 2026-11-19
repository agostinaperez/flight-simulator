// terreno_lit.vs — Vertex shader del terreno ILUMINADO (Práctico 9).
//
// A diferencia del terreno.vs del Práctico 8, acá el fragment shader necesita
// además de la coordenada de textura: la POSICIÓN y la NORMAL de cada fragmento,
// las dos en COORDENADAS DE MUNDO (el mismo sistema en el que están el sol y la
// cámara). Este vertex shader las calcula y las envía como variables out; al
// llegar al fragment shader vienen INTERPOLADAS entre los vértices del triángulo
// (la normal interpolada deja de ser unitaria y hay que volver a normalizarla).

#version 460 core

layout (location = 0) in vec3 aPos;        // posición del vértice (espacio modelo)
layout (location = 1) in vec3 aNormal;     // normal del vértice (espacio modelo)
layout (location = 2) in vec2 aTexCoord;   // coordenadas de textura

uniform mat4 uModel;                       // modelo -> mundo
uniform mat4 uView;                        // mundo -> cámara
uniform mat4 uProjection;                  // cámara -> clip
uniform float uTeselas;                    // repeticiones de la textura sobre la malla

out vec3 vPosMundo;                        // posición del fragmento, en mundo
out vec3 vNormalMundo;                     // normal del fragmento, en mundo
out vec2 vTexCoord;                        // coordenada de textura

void main()
{
    // La posición en mundo. Para el terreno uModel es la identidad (los vértices
    // ya están en mundo), pero se calcula de forma general.
    vec4 posMundo = uModel * vec4(aPos, 1.0);
    vPosMundo = posMundo.xyz;

    // La normal en mundo. El terreno NO tiene escalado no uniforme, así que la
    // matriz de modelo de la parte superior izquierda 3x3 alcanza para rotarla
    // (la traslación no afecta a una dirección). La normalización se deja para el
    // fragment shader, donde la interpolación ya la achicó.
    vNormalMundo = mat3(uModel) * aNormal;

    vTexCoord = uTeselas * aTexCoord;      // [0,1] -> [0, uTeselas]

    gl_Position = uProjection * uView * posMundo;
}
