// hud_text.vs — Vertex shader del HUD para TEXTO (dibujo instanciado).
//
// Un texto es N instancias de un MISMO cuadrilatero (dos triangulos). La malla
// base aporta la esquina del cuadrilatero (a_corner en [0,1]^2) y cada instancia
// aporta: posicion en pixeles (a_pos), tamano en pixeles (a_size), la esquina
// inferior-izquierda del glifo dentro del atlas de la fuente (a_uv), su tamano
// en UV (a_uv_size), el color (a_color) y la opacidad (a_alpha).
//
// El UV de cada vertice sale de a_uv + a_corner * a_uv_size: el mismo
// cuadrilatero "recorre" el rectangulo del glifo en el atlas. Es el metodo 2 de
// la presentacion (atributos por instancia con divisor 1).

#version 460 core

layout (location = 0) in vec2 a_corner;    // esquina del cuadrilatero (0..1)
layout (location = 1) in vec2 a_pos;       // esquina inferior-izquierda (px)
layout (location = 2) in vec2 a_size;      // tamano del glifo (px)
layout (location = 3) in vec2 a_uv;        // UV inferior-izquierda en el atlas
layout (location = 4) in vec2 a_uv_size;   // tamano del glifo en UV
layout (location = 5) in vec3 a_color;     // color del texto (RGB)
layout (location = 6) in float a_alpha;    // opacidad (por instancia)

uniform mat4 uProj;                        // proyeccion ortografica en pixeles

out vec2 v_uv;
out vec3 v_color;
out float v_alpha;

void main()
{
    // Esquina inferior-izquierda + esquina * tamano => posicion del vertice.
    vec2 p = a_pos + a_corner * a_size;
    gl_Position = uProj * vec4(p, 0.0, 1.0);
    v_uv    = a_uv + a_corner * a_uv_size;
    v_color = a_color;
    v_alpha = a_alpha;
}
