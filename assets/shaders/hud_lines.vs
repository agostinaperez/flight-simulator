// hud_lines.vs — Vertex shader del HUD para LINEAS (dibujo instanciado).
//
// Todo el trazado de lineas del HUD (escalera de cabeceo, cintas, arco de
// alabeo, simbolo de referencia) se dibuja con UNA sola llamada
// glDrawArraysInstanced. La malla "base" es un unico SEGMENTO de dos vertices
// (a_corner = 0.0 o 1.0); cada instancia aporta los DOS extremos de su propia
// linea (a_line.xy = origen, a_line.zw = fin) en PIXELES y su opacidad
// (a_alpha). El mix() coloca cada vertice del segmento entre esos dos extremos.
// Como la posicion se calcula en el CPU (build_hud) y viaja como atributo por
// instancia, este es el "metodo 2" de la presentacion (atributos con divisor).

#version 460 core

layout (location = 0) in vec2 a_corner;    // 0.0 o 1.0: a que extremo del segmento pertenece
layout (location = 1) in vec4 a_line;      // x0, y0, x1, y1 (una instancia por linea)
layout (location = 2) in float a_alpha;    // opacidad de la linea (por instancia)

uniform mat4 uProj;                        // proyeccion ortografica en pixeles

out float v_alpha;                         // llega al fragment shader

void main()
{
    // a_corner.x vale 0 (origen) o 1 (fin): mix elige el extremo correcto.
    vec2 p = mix(a_line.xy, a_line.zw, a_corner.x);
    v_alpha = a_alpha;
    gl_Position = uProj * vec4(p, 0.0, 1.0);
}
