// hud_text.fs — Fragment shader del HUD para TEXTO.
//
// SIN #version: se concatena con la linea de version. La "tinta" de cada glifo
// esta en el canal ALFA del atlas (blanco = letra, transparente = fondo). El
// color de salida es el color del HUD multiplicado por esa cobertura y por la
// opacidad de la instancia. Con la mezcla de colores activa, el borde suave del
// glifo se combina con lo que ya esta dibujado (la escena) detras del HUD.

in vec2 v_uv;
in vec3 v_color;
in float v_alpha;

layout (binding = 0) uniform sampler2D uFont;   // el atlas de la fuente 5x7

out vec4 FragColor;

void main()
{
    float cobertura = texture(uFont, v_uv).a;   // 1 = tinta, 0 = fondo
    FragColor = vec4(v_color, v_alpha * cobertura);
}
