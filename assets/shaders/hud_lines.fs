// hud_lines.fs — Fragment shader del HUD para LINEAS.
//
// SIN #version: el programa se arma concatenando "#version" + este archivo
// (igual que los fragment shaders iluminados del terreno/avion). Devuelve el
// color del HUD con la opacidad que trajo cada instancia. Las marcas cercanas a
// los bordes de las cintas llegan con alfa < 1 y se desvanecen gracias a la
// mezcla de colores (blending) que activa el modulo Hud antes de dibujar.

in float v_alpha;

uniform vec3 uColor;                       // color del HUD (RGB)

out vec4 FragColor;

void main()
{
    FragColor = vec4(uColor, v_alpha);
}
