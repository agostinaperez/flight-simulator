// terreno_lit.fs — Fragment shader del terreno ILUMINADO (Práctico 9).
//
// SIN #version: el programa se arma concatenando "#version" + iluminacion.glsl +
// este archivo (glShaderSource con varias cadenas). La textura deja de ser el
// color final y pasa a ser el coeficiente DIFUSO (kd) del material: cada texel
// es cuánto refleja la superficie, y la luz lo multiplica. Como la iluminación
// se calcula por fragmento con la normal interpolada, este es sombreado PHONG.

in vec3 vPosMundo;
in vec3 vNormalMundo;
in vec2 vTexCoord;

layout (binding = 0) uniform sampler2D uTextura;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(vNormalMundo);   // interpolada: hay que volver a normalizar

    Material m;
    m.kd = a_lineal(texture(uTextura, vTexCoord).rgb);  // la textura está en sRGB
    m.ks = vec3(0.0);                   // el pasto es MATE: sin brillo especular
    m.ns = 1.0;

    vec3 color = iluminar(vPosMundo, N, m);     // las cuentas, en lineal
    color = aplicar_niebla(color, vPosMundo);   // uNieblaColor ya está en lineal

    if (uPintarNormales) {
        // La normal como color (verificación, Parte 3): es un DATO codificado
        // como color, no un color, así que NO se le aplica gamma.
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
    } else {
        FragColor = vec4(uCorreccion ? a_srgb(color) : color, 1.0);
    }
}
